#pragma once

#include "estimated_taxes/local_storage.hpp"

#include <cstddef>
#include <memory>
#include <string>

namespace estimated_taxes {
class ApiApplication;
}

namespace estimated_taxes::http {

constexpr std::size_t kMaximumJsonBodyBytes = 1U * 1024U * 1024U;
constexpr std::size_t kMaximumRestoreBodyBytes = 64U * 1024U * 1024U;

struct ListenerConfiguration {
  std::string bind_address{"127.0.0.1"};
  unsigned short port{8080};
  std::string database_path;
  std::string backup_directory;
};

[[nodiscard]] ListenerConfiguration listener_configuration(int argc, const char* const argv[]);
[[nodiscard]] ListenerConfiguration listener_configuration(int argc, const char* const argv[],
                                                            const LocalStoragePaths& paths);

class HttpServer {
public:
  explicit HttpServer(const ListenerConfiguration& configuration);
  ~HttpServer();
  HttpServer(const HttpServer&) = delete;
  HttpServer& operator=(const HttpServer&) = delete;

  [[nodiscard]] unsigned short port() const;
  void run(const ApiApplication& application) const;

private:
  struct Listener;
  std::unique_ptr<Listener> listener_;
};

}  // namespace estimated_taxes::http
