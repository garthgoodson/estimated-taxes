#pragma once

#include <filesystem>

namespace estimated_taxes {

struct LocalStoragePaths {
  std::filesystem::path data_directory;
  std::filesystem::path database_path;
  std::filesystem::path settings_path;
  std::filesystem::path backups_directory;
};

[[nodiscard]] LocalStoragePaths local_storage_paths(const std::filesystem::path& home);
[[nodiscard]] LocalStoragePaths default_local_storage_paths();
[[nodiscard]] unsigned short load_or_create_configured_port(const LocalStoragePaths& paths);

}  // namespace estimated_taxes
