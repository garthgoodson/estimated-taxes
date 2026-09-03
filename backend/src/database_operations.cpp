#include "estimated_taxes/database_operations.hpp"

#include "estimated_taxes/input_store.hpp"
#include "estimated_taxes/recommendations.hpp"
#include "estimated_taxes/snapshot_store.hpp"
#include "estimated_taxes/sqlite.hpp"
#include "estimated_taxes/tax_rules.hpp"

#include <sqlite3.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>

namespace estimated_taxes {
namespace {

std::filesystem::path temporary_path_next_to(const std::filesystem::path& database_path, const char* suffix)
{
  const auto nonce = std::chrono::steady_clock::now().time_since_epoch().count();
  return database_path.string() + "." + std::to_string(nonce) + suffix;
}

void copy_database(sqlite3* source, sqlite3* destination)
{
  sqlite3_backup* backup = sqlite3_backup_init(destination, "main", source, "main");
  if (backup == nullptr) throw StorageError("initialize database backup failed");
  const int result = sqlite3_backup_step(backup, -1);
  const int finish_result = sqlite3_backup_finish(backup);
  if (result != SQLITE_DONE || finish_result != SQLITE_OK) throw StorageError("database backup failed");
}

void validate_database_file(const std::string& path, const std::string& as_of_date)
{
  sqlite3* raw_database{};
  if (sqlite3_open_v2(path.c_str(), &raw_database, SQLITE_OPEN_READONLY, nullptr) != SQLITE_OK) {
    if (raw_database != nullptr) sqlite3_close(raw_database);
    throw ValidationError("restore is not a readable SQLite database");
  }
  bool valid{};
  {
    sqlite::Statement integrity(raw_database, "PRAGMA integrity_check");
    valid = integrity.step_row() && integrity.required_text(0, "integrity result") == "ok";
  }
  sqlite3_close(raw_database);
  if (!valid) throw ValidationError("restore database failed its integrity check");

  InputStore inputs(path);
  RuleStore rules(path);
  SnapshotStore snapshots(path);
  const Household household = inputs.load_household();
  TaxYearInputs tax_year_inputs;
  for (int quarter = 1; quarter <= 4; ++quarter) tax_year_inputs.quarters[quarter - 1] = inputs.load_quarter(quarter);
  const ActiveRules active_rules = rules.load_active();
  (void)compose_current_result(household, tax_year_inputs, as_of_date, active_rules);
  (void)snapshots.list();
}

class RemoveOnExit {
public:
  explicit RemoveOnExit(std::filesystem::path path) : path_(std::move(path)) {}
  ~RemoveOnExit() { std::filesystem::remove(path_); }
private:
  std::filesystem::path path_;
};

}  // namespace

std::string backup_database(const std::string& database_path)
{
  const std::filesystem::path temporary = temporary_path_next_to(database_path, ".backup");
  RemoveOnExit cleanup(temporary);
  sqlite::Connection source(database_path);
  sqlite::Connection destination(temporary.string());
  copy_database(source.get(), destination.get());

  std::ifstream input(temporary, std::ios::binary);
  if (!input) throw StorageError("open completed backup failed");
  return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}

void restore_database(const std::string& database_path, const std::string& database_image,
                      const std::string& as_of_date)
{
  const std::filesystem::path target(database_path);
  const std::filesystem::path staged = temporary_path_next_to(target, ".restore");
  RemoveOnExit staged_cleanup(staged);

  {
    std::ofstream output(staged, std::ios::binary | std::ios::trunc);
    if (!output || !output.write(database_image.data(), static_cast<std::streamsize>(database_image.size()))) {
      throw StorageError("stage restore database failed");
    }
  }
  try {
    validate_database_file(staged.string(), as_of_date);
  } catch (const ValidationError&) {
    throw;
  } catch (const std::exception&) {
    throw ValidationError("restore database is invalid or incompatible");
  }

  sqlite::Connection source(staged.string());
  sqlite::Connection destination(target.string());
  copy_database(source.get(), destination.get());
}

}  // namespace estimated_taxes
