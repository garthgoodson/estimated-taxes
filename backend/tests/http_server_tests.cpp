#include "estimated_taxes/api.hpp"
#include "estimated_taxes/http_server.hpp"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>

#include <array>
#include <cerrno>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <utility>

using namespace estimated_taxes;
using namespace estimated_taxes::http;
namespace {

class FixedClock final : public CurrentDateProvider {
public:
  [[nodiscard]] std::string current_date() const override { return "2026-03-31"; }
};

void require(bool condition, const char* message)
{
  if (!condition) throw std::runtime_error(message);
}

void configuration()
{
  const auto home = std::filesystem::temp_directory_path() / "estimated_taxes_http_configuration_test";
  std::filesystem::remove_all(home);
  const LocalStoragePaths paths = local_storage_paths(home);
  const char* defaults[] = {"backend"};
  const auto standard = listener_configuration(1, defaults, paths);
  require(standard.bind_address == "127.0.0.1" && standard.port == 8080 &&
              standard.database_path == paths.database_path.string() &&
              std::filesystem::exists(paths.settings_path),
          "local storage defaults");

  std::ofstream settings(paths.settings_path, std::ios::trunc);
  settings << "{\"port\": 9080}";
  settings.close();
  const auto stored = listener_configuration(1, defaults, paths);
  require(stored.port == 9080, "stored settings port used");
  const char* configured[] = {"backend", "--port", "9081", "--database", "/tmp/taxes.sqlite"};
  const auto overridden = listener_configuration(5, configured, paths);
  require(overridden.bind_address == "127.0.0.1" && overridden.port == 9081 &&
              overridden.database_path == "/tmp/taxes.sqlite" &&
              overridden.backup_directory == paths.backups_directory.string(),
          "command-line overrides remain loopback");

  settings.open(paths.settings_path, std::ios::trunc);
  settings << "{}";
  settings.close();
  bool rejected{};
  try {
    (void)listener_configuration(1, defaults, paths);
  } catch (const std::exception&) {
    rejected = true;
  }
  require(rejected, "invalid settings rejected");
  std::filesystem::remove_all(home);
}

int connect_with_retry(unsigned short port)
{
  for (int attempt = 0; attempt < 100; ++attempt) {
    const int socket_handle = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_handle < 0) return -1;
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    address.sin_port = htons(port);
    if (connect(socket_handle, reinterpret_cast<sockaddr*>(&address), sizeof(address)) == 0) return socket_handle;
    close(socket_handle);
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
  return -1;
}

std::string exchange(unsigned short port, const std::string& request)
{
  const int client = connect_with_retry(port);
  require(client >= 0, "connect to listener");
  std::string_view unsent(request);
  while (!unsent.empty()) {
    const ssize_t sent = send(client, unsent.data(), unsent.size(), 0);
    require(sent > 0, "send request");
    unsent.remove_prefix(static_cast<std::size_t>(sent));
  }
  std::string response;
  char buffer[4096];
  for (;;) {
    const ssize_t count = recv(client, buffer, sizeof(buffer), 0);
    if (count > 0) response.append(buffer, static_cast<std::size_t>(count));
    else if (count < 0 && errno == EINTR) continue;
    else break;
  }
  close(client);
  return response;
}

void listener_dispatches_to_application()
{
  const auto database = std::filesystem::temp_directory_path() / "estimated_taxes_http_listener_test.sqlite";
  std::filesystem::remove(database);
  ListenerConfiguration configuration;
  configuration.port = 0;
  configuration.database_path = database.string();
  HttpServer server(configuration);

  const pid_t child = fork();
  require(child >= 0, "fork listener process");
  if (child == 0) {
    try {
      FixedClock clock;
      ApiApplication application(configuration.database_path, configuration.backup_directory, clock);
      server.run(application);
    } catch (...) {
      _exit(2);
    }
  }

  try {
    const std::string response = exchange(server.port(), "GET /api/2026/household HTTP/1.1\r\nHost: 127.0.0.1\r\n\r\n");
    require(response.starts_with("HTTP/1.1 200 OK") && response.find("\"tax_year\":2026") != std::string::npos,
            "listener dispatches API response");
    const std::string body = "{\"label\":\"Chunked\"}";
    std::ostringstream chunked;
    chunked << "POST /api/2026/snapshots HTTP/1.1\r\nHost: 127.0.0.1\r\n"
            << "Content-Type: application/json\r\nTransfer-Encoding: chunked\r\n\r\n"
            << std::hex << body.size() << "\r\n" << body << "\r\n0\r\n\r\n";
    const std::string chunked_response = exchange(server.port(), chunked.str());
    require(chunked_response.starts_with("HTTP/1.1 201 Created") && chunked_response.find("Chunked") != std::string::npos,
            "listener decodes chunked request body");

    const std::string oversized_json =
        "PUT /api/2026/quarters/1 HTTP/1.1\r\nHost: 127.0.0.1\r\nContent-Type: application/json\r\nContent-Length: " +
        std::to_string(kMaximumJsonBodyBytes + 1) + "\r\n\r\n";
    require(exchange(server.port(), oversized_json).starts_with("HTTP/1.1 413 Payload Too Large"),
            "listener rejects oversized declared JSON body");
    const std::string oversized_restore =
        "POST /api/restore HTTP/1.1\r\nHost: 127.0.0.1\r\nContent-Type: application/octet-stream\r\nContent-Length: " +
        std::to_string(kMaximumRestoreBodyBytes + 1) + "\r\n\r\n";
    require(exchange(server.port(), oversized_restore).starts_with("HTTP/1.1 413 Payload Too Large"),
            "listener rejects oversized declared restore body");
  } catch (...) {
    kill(child, SIGTERM);
    waitpid(child, nullptr, 0);
    std::filesystem::remove(database);
    throw;
  }
  kill(child, SIGTERM);
  waitpid(child, nullptr, 0);
  std::filesystem::remove(database);
}

}  // namespace

int main()
{
  using TestCase = std::pair<const char*, void (*)()>;
  const std::array<TestCase, 2> tests{{
    {"configuration", configuration},
    {"listener dispatch", listener_dispatches_to_application},
  }};

  int failures{};
  for (const auto& [name, test] : tests) {
    try {
      test();
      std::cout << "PASS: " << name << '\n';
    } catch (const std::exception& error) {
      ++failures;
      std::cerr << "FAIL: " << name << ": " << error.what() << '\n';
    }
  }
  return failures;
}
