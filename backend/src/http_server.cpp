#include "estimated_taxes/http_server.hpp"

#include "estimated_taxes/api.hpp"
#include "estimated_taxes/input_store.hpp"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <algorithm>
#include <array>
#include <charconv>
#include <cerrno>
#include <cctype>
#include <cstring>
#include <iostream>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string_view>

namespace estimated_taxes::http {
namespace {

constexpr std::size_t kMaximumHeaderBytes = 64U * 1024U;
constexpr std::size_t kMaximumChunkFramingBytes = 64U * 1024U;

class ProtocolError : public std::runtime_error {
public:
  using std::runtime_error::runtime_error;
};

class PayloadTooLarge : public std::runtime_error {
public:
  PayloadTooLarge() : std::runtime_error("request body is too large") {}
};

unsigned short parse_port(std::string_view value)
{
  unsigned int port{};
  const auto [end, error] = std::from_chars(value.data(), value.data() + value.size(), port);
  if (error != std::errc{} || end != value.data() + value.size() || port == 0 || port > 65535) {
    throw ValidationError("port must be between 1 and 65535");
  }
  return static_cast<unsigned short>(port);
}

std::string lowercase(std::string value)
{
  std::transform(value.begin(), value.end(), value.begin(), [](unsigned char character) {
    return static_cast<char>(std::tolower(character));
  });
  return value;
}

std::string trim(std::string value)
{
  const auto first = value.find_first_not_of(" \t");
  if (first == std::string::npos) return {};
  const auto last = value.find_last_not_of(" \t");
  return value.substr(first, last - first + 1);
}

std::string status_text(int status)
{
  switch (status) {
    case 200: return "OK";
    case 201: return "Created";
    case 204: return "No Content";
    case 400: return "Bad Request";
    case 404: return "Not Found";
    case 413: return "Payload Too Large";
    case 415: return "Unsupported Media Type";
    case 422: return "Unprocessable Content";
    default: return "Internal Server Error";
  }
}

void send_all(int socket, std::string_view bytes)
{
  while (!bytes.empty()) {
    const ssize_t count = send(socket, bytes.data(), bytes.size(), 0);
    if (count < 0) {
      if (errno == EINTR) continue;
      throw std::runtime_error("send response failed");
    }
    bytes.remove_prefix(static_cast<std::size_t>(count));
  }
}

void send_response(int socket, const ApiResponse& response)
{
  std::ostringstream headers;
  headers << "HTTP/1.1 " << response.status << ' ' << status_text(response.status) << "\r\n";
  for (const auto& [name, value] : response.headers) headers << name << ": " << value << "\r\n";
  headers << "Content-Length: " << response.body.size() << "\r\nConnection: close\r\n\r\n";
  send_all(socket, headers.str());
  send_all(socket, response.body);
}

ApiResponse protocol_error(int status, const char* code, const char* message)
{
  return {status, {{"Content-Type", "application/json"}},
          std::string("{\"error\":{\"code\":\"") + code + "\",\"message\":\"" + message + "\"}}"};
}

std::optional<std::size_t> parse_content_length(const std::map<std::string, std::string>& headers)
{
  const auto iterator = headers.find("content-length");
  if (iterator == headers.end()) return std::nullopt;
  std::size_t value{};
  const std::string& text = iterator->second;
  const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);
  if (error != std::errc{} || end != text.data() + text.size()) throw ProtocolError("Content-Length is invalid");
  return value;
}

void receive_more(int socket, std::string& wire, std::size_t maximum_wire_bytes)
{
  if (wire.size() >= maximum_wire_bytes) throw PayloadTooLarge();
  std::array<char, 8192> buffer{};
  const std::size_t capacity = std::min(buffer.size(), maximum_wire_bytes - wire.size());
  const ssize_t count = recv(socket, buffer.data(), capacity, 0);
  if (count <= 0) throw ProtocolError("request body is incomplete");
  wire.append(buffer.data(), static_cast<std::size_t>(count));
}

std::string read_chunked_body(int socket, std::string wire, std::size_t limit)
{
  std::string decoded;
  std::size_t cursor{};
  const std::size_t maximum_wire_bytes = limit + kMaximumChunkFramingBytes;
  for (;;) {
    std::size_t line_end{};
    while ((line_end = wire.find("\r\n", cursor)) == std::string::npos) {
      if (wire.size() - cursor > 1024) throw ProtocolError("chunk header is too long");
      receive_more(socket, wire, maximum_wire_bytes);
    }
    std::string_view size_text(wire.data() + cursor, line_end - cursor);
    if (const auto extension = size_text.find(';'); extension != std::string_view::npos) size_text = size_text.substr(0, extension);
    std::size_t chunk_size{};
    const auto [end, error] = std::from_chars(size_text.data(), size_text.data() + size_text.size(), chunk_size, 16);
    if (size_text.empty() || error != std::errc{} || end != size_text.data() + size_text.size()) {
      throw ProtocolError("chunk size is invalid");
    }
    cursor = line_end + 2;
    if (chunk_size == 0) {
      for (;;) {
        while ((line_end = wire.find("\r\n", cursor)) == std::string::npos) {
          receive_more(socket, wire, maximum_wire_bytes);
        }
        if (line_end == cursor) return decoded;
        cursor = line_end + 2;
      }
    }
    if (chunk_size > limit - decoded.size()) throw PayloadTooLarge();
    while (wire.size() - cursor < chunk_size + 2) receive_more(socket, wire, maximum_wire_bytes);
    decoded.append(wire.data() + cursor, chunk_size);
    cursor += chunk_size;
    if (wire.compare(cursor, 2, "\r\n") != 0) throw ProtocolError("chunk data is invalid");
    cursor += 2;
    if (cursor > 8192) {
      wire.erase(0, cursor);
      cursor = 0;
    }
  }
}

void handle_connection(int socket, const ApiApplication& application)
{
  std::string received;
  std::array<char, 8192> buffer{};
  std::size_t header_end = std::string::npos;
  while ((header_end = received.find("\r\n\r\n")) == std::string::npos) {
    const ssize_t count = recv(socket, buffer.data(), buffer.size(), 0);
    if (count <= 0) throw std::runtime_error("read request headers failed");
    received.append(buffer.data(), static_cast<std::size_t>(count));
    if (received.size() > kMaximumHeaderBytes) {
      send_response(socket, protocol_error(400, "invalid_request", "The request headers are too large."));
      return;
    }
  }

  std::istringstream lines(received.substr(0, header_end));
  ApiRequest request;
  std::string version;
  if (!(lines >> request.method >> request.path >> version) || version != "HTTP/1.1") {
    send_response(socket, protocol_error(400, "invalid_request", "The HTTP request line is invalid."));
    return;
  }
  std::string line;
  std::getline(lines, line);
  while (std::getline(lines, line)) {
    if (!line.empty() && line.back() == '\r') line.pop_back();
    const auto separator = line.find(':');
    if (separator == std::string::npos) {
      send_response(socket, protocol_error(400, "invalid_request", "An HTTP header is invalid."));
      return;
    }
    request.headers[lowercase(trim(line.substr(0, separator)))] = trim(line.substr(separator + 1));
  }

  try {
    const std::optional<std::size_t> content_length = parse_content_length(request.headers);
    const auto transfer_encoding = request.headers.find("transfer-encoding");
    const bool chunked = transfer_encoding != request.headers.end() && lowercase(transfer_encoding->second) == "chunked";
    if (transfer_encoding != request.headers.end() && !chunked) throw ProtocolError("Transfer-Encoding is unsupported");
    if (content_length && chunked) throw ProtocolError("request has conflicting body framing");
    const bool expects_body = request.method == "PUT" || request.method == "POST";
    if (expects_body && !content_length && !chunked) throw ProtocolError("request body framing is required");
    const std::size_t limit = request.path == "/api/restore" ? kMaximumRestoreBodyBytes : kMaximumJsonBodyBytes;
    if (content_length && *content_length > limit) throw PayloadTooLarge();

    request.body = received.substr(header_end + 4);
    if (chunked) {
      request.body = read_chunked_body(socket, std::move(request.body), limit);
    } else {
      while (content_length && request.body.size() < *content_length) {
        const std::size_t remaining = *content_length - request.body.size();
        const ssize_t count = recv(socket, buffer.data(), std::min(buffer.size(), remaining), 0);
        if (count <= 0) throw ProtocolError("request body is incomplete");
        request.body.append(buffer.data(), static_cast<std::size_t>(count));
      }
      if (content_length && request.body.size() > *content_length) request.body.resize(*content_length);
    }
  } catch (const PayloadTooLarge&) {
    send_response(socket, protocol_error(413, "payload_too_large", "The request body is too large."));
    return;
  } catch (const ProtocolError&) {
    send_response(socket, protocol_error(400, "invalid_request", "The HTTP request body is invalid."));
    return;
  }
  send_response(socket, application.handle(request));
}

}  // namespace

ListenerConfiguration listener_configuration(int argc, const char* const argv[])
{
  ListenerConfiguration configuration;
  for (int index = 1; index < argc; ++index) {
    const std::string_view argument(argv[index]);
    if (argument == "--port") {
      if (++index == argc) throw ValidationError("--port requires a value");
      configuration.port = parse_port(argv[index]);
      continue;
    }
    if (argument == "--database") {
      if (++index == argc || std::string_view(argv[index]).empty()) throw ValidationError("--database requires a value");
      configuration.database_path = argv[index];
      continue;
    }
    constexpr std::string_view port_prefix = "--port=";
    if (argument.starts_with(port_prefix)) {
      configuration.port = parse_port(argument.substr(port_prefix.size()));
      continue;
    }
    constexpr std::string_view database_prefix = "--database=";
    if (argument.starts_with(database_prefix) && argument.size() > database_prefix.size()) {
      configuration.database_path = argument.substr(database_prefix.size());
      continue;
    }
    throw ValidationError("unsupported command-line option");
  }
  return configuration;
}

struct HttpServer::Listener {
  int socket{-1};
  unsigned short port{};

  ~Listener()
  {
    if (socket >= 0) close(socket);
  }
};

HttpServer::HttpServer(const ListenerConfiguration& configuration)
    : listener_(std::make_unique<Listener>())
{
  listener_->socket = socket(AF_INET, SOCK_STREAM, 0);
  if (listener_->socket < 0) throw std::runtime_error("create listener failed");
  const int reuse = 1;
  setsockopt(listener_->socket, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
  sockaddr_in address{};
  address.sin_family = AF_INET;
  address.sin_port = htons(configuration.port);
  if (inet_pton(AF_INET, configuration.bind_address.c_str(), &address.sin_addr) != 1 ||
      bind(listener_->socket, reinterpret_cast<sockaddr*>(&address), sizeof(address)) != 0 ||
      listen(listener_->socket, 16) != 0) {
    throw std::runtime_error("bind loopback listener failed");
  }
  socklen_t address_size = sizeof(address);
  if (getsockname(listener_->socket, reinterpret_cast<sockaddr*>(&address), &address_size) != 0) {
    throw std::runtime_error("read listener address failed");
  }
  listener_->port = ntohs(address.sin_port);
}

HttpServer::~HttpServer() = default;

unsigned short HttpServer::port() const { return listener_->port; }

void HttpServer::run(const ApiApplication& application) const
{
  for (;;) {
    const int connection = accept(listener_->socket, nullptr, nullptr);
    if (connection < 0) {
      if (errno == EINTR) continue;
      throw std::runtime_error("accept connection failed");
    }
    try {
      handle_connection(connection, application);
    } catch (const std::exception& error) {
      try {
        send_response(connection, protocol_error(500, "internal_error", "The request could not be completed."));
      } catch (...) {
      }
      std::cerr << "HTTP request failed: " << error.what() << '\n';
    }
    close(connection);
  }
}

}  // namespace estimated_taxes::http
