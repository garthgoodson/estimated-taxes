#pragma once

#include "estimated_taxes/current_date_provider.hpp"

#include <map>
#include <memory>
#include <string>

namespace estimated_taxes {

struct ApiRequest {
  std::string method;
  std::string path;
  std::map<std::string, std::string> headers;
  std::string body;
};

struct ApiResponse {
  int status{};
  std::map<std::string, std::string> headers;
  std::string body;
};

class ApiApplication {
public:
  ApiApplication(std::string database_path, std::string backup_directory, const CurrentDateProvider& clock);
  [[nodiscard]] ApiResponse handle(const ApiRequest& request) const;

private:
  std::string database_path_;
  std::string backup_directory_;
  const CurrentDateProvider& clock_;
};

}  // namespace estimated_taxes
