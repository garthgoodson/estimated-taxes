#pragma once

#include <string>

namespace estimated_taxes {

[[nodiscard]] std::string backup_database(const std::string& database_path);
void restore_database(const std::string& database_path, const std::string& database_image,
                      const std::string& as_of_date);

}  // namespace estimated_taxes
