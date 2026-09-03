#include "estimated_taxes/input_store.hpp"

#include <sqlite3.h>

#include <filesystem>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>

namespace fs = std::filesystem;
using estimated_taxes::Cents;
using estimated_taxes::EstimatedPayment;
using estimated_taxes::Household;
using estimated_taxes::InputStore;
using estimated_taxes::InvestmentSummary;
using estimated_taxes::PaystubSnapshot;
using estimated_taxes::QuarterInput;
using estimated_taxes::SpouseKey;
using estimated_taxes::StorageError;
using estimated_taxes::ValidationError;

namespace {

class TemporaryDatabase {
public:
  TemporaryDatabase()
  {
    static int counter = 0;
    path_ = fs::temp_directory_path() / ("estimated-taxes-b1-" + std::to_string(++counter) + ".sqlite");
    fs::remove(path_);
  }

  ~TemporaryDatabase()
  {
    fs::remove(path_);
  }

  const std::string path() const { return path_.string(); }

private:
  fs::path path_;
};

void require(bool condition, const std::string& message)
{
  if (!condition) {
    throw std::runtime_error(message);
  }
}

template <typename Exception, typename Function>
void require_throws(Function&& function, const std::string& message)
{
  try {
    std::forward<Function>(function)();
  } catch (const Exception&) {
    return;
  }
  throw std::runtime_error(message);
}

PaystubSnapshot paystub(Cents base = 100'000)
{
  return PaystubSnapshot{
      "2026-03-15", "biweekly", base, 0, 10'000, 5'000, base * 3, base * 3,
      30'000, 15'000, 6'000, 1'500, 800,
  };
}

QuarterInput complete_quarter(int quarter = 1)
{
  QuarterInput input;
  input.quarter = quarter;
  input.spouse_1_paystub = paystub();
  input.spouse_2_paystub = paystub(80'000);
  input.investments = InvestmentSummary{40'000, 35'000, -12'000, 120'000, 0, 0, "quarter notes"};
  input.federal_payment = EstimatedPayment{50'000, "2026-04-15"};
  input.california_payment = EstimatedPayment{20'000, "2026-04-15"};
  return input;
}

void test_new_database_initialization_and_migration_tracking()
{
  TemporaryDatabase database;
  InputStore store(database.path());
  require(store.schema_version() == 1, "new database should record migration version 1");
  require(store.load_household() == Household{}, "new database should seed the fixed household");
  for (int quarter = 1; quarter <= 4; ++quarter) {
    require(store.load_quarter(quarter) == QuarterInput{quarter}, "new database should seed empty quarters");
  }
}

void test_reopening_existing_database()
{
  TemporaryDatabase database;
  {
    InputStore store(database.path());
    Household household;
    household.spouse_1.label = "Alex";
    household.spouse_2.label = "Casey";
    household.spouse_2.blind = true;
    store.replace_household(household);
    store.replace_quarter(complete_quarter(2));
  }
  InputStore reopened(database.path());
  require(reopened.schema_version() == 1, "reopening must preserve migration version");
  require(reopened.load_household().spouse_1.label == "Alex", "reopening must preserve household");
  require(reopened.load_quarter(2) == complete_quarter(2), "reopening must preserve quarter input");
}

void test_household_round_trip_and_stable_identities()
{
  TemporaryDatabase database;
  InputStore store(database.path());
  Household household;
  household.spouse_1.label = "Alex";
  household.spouse_1.age_65_or_older = true;
  household.spouse_2.label = "Casey";
  household.spouse_2.blind = true;
  store.replace_household(household);
  require(store.load_household() == household, "household should round trip");

  Household invalid = household;
  invalid.spouse_1.key = SpouseKey::spouse_2;
  require_throws<ValidationError>([&] { store.replace_household(invalid); }, "invalid spouse identity must be rejected");
}

void test_complete_and_optional_paystub_round_trip()
{
  TemporaryDatabase database;
  InputStore store(database.path());
  const QuarterInput complete = complete_quarter();
  store.replace_quarter(complete);
  require(store.load_quarter(1) == complete, "complete quarter should round trip");

  QuarterInput only_first;
  only_first.quarter = 3;
  only_first.spouse_1_paystub = paystub();
  store.replace_quarter(only_first);
  require(store.load_quarter(3) == only_first, "optional spouse paystub should round trip");
}

void test_zero_missing_negative_and_payment_separation()
{
  TemporaryDatabase database;
  InputStore store(database.path());
  QuarterInput input;
  input.quarter = 1;
  input.investments = InvestmentSummary{0, 0, -77, 0, 0, 0, std::nullopt};
  input.federal_payment = EstimatedPayment{0, std::nullopt};
  input.california_payment = EstimatedPayment{99, "2026-04-15"};
  store.replace_quarter(input);

  const QuarterInput loaded = store.load_quarter(1);
  require(loaded.investments.has_value(), "explicit zero investment summary must not become missing");
  require(loaded.investments->ordinary_dividends_cents == 0, "explicit zero must round trip");
  require(loaded.investments->short_term_gain_cents == -77, "negative gain/loss must round trip");
  require(!loaded.federal_payment.date && loaded.federal_payment.amount_cents == 0,
          "missing payment must remain distinct from payment values");
  require(loaded.california_payment.amount_cents == 99 && loaded.federal_payment.amount_cents == 0,
          "federal and California payments must remain separate");
}

void test_replacement_clears_removed_values()
{
  TemporaryDatabase database;
  InputStore store(database.path());
  store.replace_quarter(complete_quarter());
  QuarterInput replacement;
  replacement.quarter = 1;
  replacement.spouse_2_paystub = paystub(90'000);
  replacement.california_payment = EstimatedPayment{10'000, "2026-04-15"};
  store.replace_quarter(replacement);
  require(store.load_quarter(1) == replacement, "quarter replacement must clear omitted optional values");
}

void test_invalid_quarters_and_domain_constraints()
{
  TemporaryDatabase database;
  InputStore store(database.path());
  require_throws<ValidationError>([&] { static_cast<void>(store.load_quarter(5)); }, "invalid quarter read must be rejected");

  QuarterInput invalid;
  invalid.quarter = 0;
  require_throws<ValidationError>([&] { store.replace_quarter(invalid); }, "invalid quarter save must be rejected");

  const QuarterInput original = complete_quarter();
  store.replace_quarter(original);
  invalid = QuarterInput{1};
  invalid.investments = InvestmentSummary{10, 11, 0, 0, 0, 0, std::nullopt};
  require_throws<ValidationError>([&] { store.replace_quarter(invalid); }, "qualified dividends cannot exceed ordinary dividends");
  require(store.load_quarter(1) == original, "failed validation must leave the prior quarter unchanged");
}

void test_one_paystub_per_spouse_per_quarter()
{
  TemporaryDatabase database;
  InputStore store(database.path());
  store.replace_quarter(complete_quarter());

  sqlite3* raw = nullptr;
  require(sqlite3_open(database.path().c_str(), &raw) == SQLITE_OK, "open test database");
  const char* sql =
      "INSERT INTO paystubs SELECT * FROM paystubs WHERE quarter = 1 AND spouse_key = 'spouse_1';";
  require(sqlite3_exec(raw, sql, nullptr, nullptr, nullptr) == SQLITE_CONSTRAINT,
          "database must enforce one paystub per spouse per quarter");
  sqlite3_close(raw);
}

void test_transaction_rollback_after_database_failure()
{
  TemporaryDatabase database;
  InputStore store(database.path());
  const QuarterInput original = complete_quarter();
  store.replace_quarter(original);

  sqlite3* raw = nullptr;
  require(sqlite3_open(database.path().c_str(), &raw) == SQLITE_OK, "open test database");
  require(sqlite3_exec(raw,
                       "CREATE TRIGGER reject_investment BEFORE INSERT ON investments "
                       "BEGIN SELECT RAISE(ABORT, 'forced failure'); END;",
                       nullptr, nullptr, nullptr) == SQLITE_OK,
          "create failure trigger");
  sqlite3_close(raw);

  QuarterInput replacement = complete_quarter();
  replacement.spouse_1_paystub->current_period_regular_wages_cents = 200'000;
  require_throws<StorageError>([&] { store.replace_quarter(replacement); }, "forced database failure must propagate");
  require(store.load_quarter(1) == original, "failed replacement must roll back the complete prior quarter");
}

}  // namespace

int main()
{
  const std::pair<const char*, std::function<void()>> tests[] = {
      {"new database initialization and migration tracking", test_new_database_initialization_and_migration_tracking},
      {"reopening existing database", test_reopening_existing_database},
      {"household round trip and stable identities", test_household_round_trip_and_stable_identities},
      {"complete and optional paystub round trip", test_complete_and_optional_paystub_round_trip},
      {"zero, missing, negative, and payment separation", test_zero_missing_negative_and_payment_separation},
      {"replacement clears removed values", test_replacement_clears_removed_values},
      {"invalid quarters and domain constraints", test_invalid_quarters_and_domain_constraints},
      {"one paystub per spouse per quarter", test_one_paystub_per_spouse_per_quarter},
      {"transaction rollback after database failure", test_transaction_rollback_after_database_failure},
  };

  int failures = 0;
  for (const auto& [name, test] : tests) {
    try {
      test();
      std::cout << "PASS: " << name << '\n';
    } catch (const std::exception& error) {
      ++failures;
      std::cerr << "FAIL: " << name << ": " << error.what() << '\n';
    }
  }
  return failures == 0 ? 0 : 1;
}
