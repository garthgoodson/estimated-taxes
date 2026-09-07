#include "estimated_taxes/local_storage.hpp"

#include "estimated_taxes/input_store.hpp"

#include <jansson.h>

#include <cstdlib>
#include <fstream>
#include <memory>
#include <string>

namespace estimated_taxes {
namespace {

constexpr unsigned short kDefaultPort = 8080;

[[nodiscard]] json_t* load_settings(const std::filesystem::path& path)
{
  json_error_t error{};
  json_t* settings = json_load_file(path.c_str(), JSON_REJECT_DUPLICATES, &error);
  if (settings == nullptr) throw ValidationError("settings.json is invalid");
  return settings;
}

void write_default_settings(const std::filesystem::path& path)
{
  std::ofstream output(path, std::ios::trunc);
  if (!output) throw StorageError("create settings.json failed");
  output << "{\n  \"port\": " << kDefaultPort << "\n}\n";
  if (!output) throw StorageError("write settings.json failed");
}

}  // namespace

LocalStoragePaths local_storage_paths(const std::filesystem::path& home)
{
  if (home.empty()) throw StorageError("HOME is required for local application storage");

  const std::filesystem::path data_directory = home / ".fi-estaxes";
  std::error_code error;
  std::filesystem::create_directories(data_directory, error);
  if (error) throw StorageError("create local application data directory failed");

  return {data_directory, data_directory / "estimated-taxes.sqlite", data_directory / "settings.json",
          data_directory / "backups"};
}

LocalStoragePaths default_local_storage_paths()
{
  const char* home = std::getenv("HOME");
  return local_storage_paths(home == nullptr ? std::filesystem::path{} : std::filesystem::path(home));
}

unsigned short load_or_create_configured_port(const LocalStoragePaths& paths)
{
  if (!std::filesystem::exists(paths.settings_path)) write_default_settings(paths.settings_path);

  json_t* settings = load_settings(paths.settings_path);
  const auto release = [](json_t* value) { json_decref(value); };
  std::unique_ptr<json_t, decltype(release)> settings_guard(settings, release);
  if (!json_is_object(settings) || json_object_size(settings) != 1) {
    throw ValidationError("settings.json must contain only port");
  }
  json_t* port = json_object_get(settings, "port");
  if (!json_is_integer(port)) throw ValidationError("settings.json port must be an integer");
  const json_int_t value = json_integer_value(port);
  if (value < 1 || value > 65535) throw ValidationError("settings.json port must be between 1 and 65535");
  return static_cast<unsigned short>(value);
}

}  // namespace estimated_taxes
