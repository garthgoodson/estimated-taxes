#include "estimated_taxes/input_store.hpp"
#include "estimated_taxes/sqlite.hpp"

#include <sqlite3.h>

#include <array>
#include <memory>
#include <sstream>
#include <string_view>
#include <utility>

namespace estimated_taxes {
namespace {

constexpr int kSchemaVersion = 3;

[[noreturn]] void throw_sqlite_error(sqlite3* database, std::string_view operation)
{
  throw StorageError(std::string(operation) + ": " + sqlite3_errmsg(database));
}

void execute(sqlite3* database, const char* sql)
{
  char* error_message = nullptr;
  if (sqlite3_exec(database, sql, nullptr, nullptr, &error_message) != SQLITE_OK) {
    const std::string message = error_message == nullptr ? sqlite3_errmsg(database) : error_message;
    sqlite3_free(error_message);
    throw StorageError(message);
  }
}

class Statement {
public:
  Statement(sqlite3* database, const char* sql)
  {
    if (sqlite3_prepare_v2(database, sql, -1, &statement_, nullptr) != SQLITE_OK) {
      throw_sqlite_error(database, "prepare statement");
    }
  }

  ~Statement()
  {
    sqlite3_finalize(statement_);
  }

  sqlite3_stmt* get() const { return statement_; }

private:
  sqlite3_stmt* statement_{};
};

void bind_integer(sqlite3_stmt* statement, int index, Cents value)
{
  if (sqlite3_bind_int64(statement, index, value) != SQLITE_OK) {
    throw StorageError("bind integer failed");
  }
}

void bind_text(sqlite3_stmt* statement, int index, const std::string& value)
{
  if (sqlite3_bind_text(statement, index, value.c_str(), -1, SQLITE_TRANSIENT) != SQLITE_OK) {
    throw StorageError("bind text failed");
  }
}

void bind_optional_text(sqlite3_stmt* statement, int index, const std::optional<std::string>& value)
{
  if (value) {
    bind_text(statement, index, *value);
  } else if (sqlite3_bind_null(statement, index) != SQLITE_OK) {
    throw StorageError("bind null failed");
  }
}

void expect_done(sqlite3* database, sqlite3_stmt* statement, std::string_view operation)
{
  if (sqlite3_step(statement) != SQLITE_DONE) {
    throw_sqlite_error(database, operation);
  }
}

std::string_view spouse_key_name(SpouseKey key)
{
  return key == SpouseKey::spouse_1 ? "spouse_1" : "spouse_2";
}

SpouseKey spouse_key_from_name(const unsigned char* key)
{
  const std::string_view value(reinterpret_cast<const char*>(key));
  if (value == "spouse_1") {
    return SpouseKey::spouse_1;
  }
  if (value == "spouse_2") {
    return SpouseKey::spouse_2;
  }
  throw StorageError("stored invalid spouse key");
}

bool is_valid_date(const std::string& value)
{
  if (value.size() != 10 || value[4] != '-' || value[7] != '-') {
    return false;
  }
  for (const int index : {0, 1, 2, 3, 5, 6, 8, 9}) {
    if (value[index] < '0' || value[index] > '9') {
      return false;
    }
  }
  const int year = std::stoi(value.substr(0, 4));
  const int month = std::stoi(value.substr(5, 2));
  const int day = std::stoi(value.substr(8, 2));
  if (year != 2026 || month < 1 || month > 12) {
    return false;
  }
  constexpr int days_per_month[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  return day >= 1 && day <= days_per_month[month - 1];
}

bool is_valid_frequency(const std::string& frequency)
{
  return frequency == "weekly" || frequency == "biweekly" || frequency == "semimonthly" ||
         frequency == "monthly";
}

void validate_nonnegative(Cents value, std::string_view field)
{
  if (value < 0) {
    throw ValidationError(std::string(field) + " cannot be negative");
  }
}

void validate(const PaystubSnapshot& paystub)
{
  if (!is_valid_date(paystub.date)) {
    throw ValidationError("paystub date must use YYYY-MM-DD");
  }
  if (!is_valid_frequency(paystub.pay_frequency)) {
    throw ValidationError("paystub pay frequency is invalid");
  }
  for (const auto [value, field] : std::array{
           std::pair{paystub.current_period_regular_wages_cents, "current-period regular wages"},
           std::pair{paystub.current_period_bonus_wages_cents, "current-period bonus wages"},
           std::pair{paystub.current_period_federal_withholding_cents, "current-period federal withholding"},
           std::pair{paystub.current_period_california_withholding_cents, "current-period California withholding"},
           std::pair{paystub.federal_taxable_wages_ytd_cents, "federal taxable wages YTD"},
           std::pair{paystub.california_taxable_wages_ytd_cents, "California taxable wages YTD"},
           std::pair{paystub.federal_withholding_ytd_cents, "federal withholding YTD"},
           std::pair{paystub.california_withholding_ytd_cents, "California withholding YTD"},
           std::pair{paystub.social_security_withholding_ytd_cents, "Social Security withholding YTD"},
           std::pair{paystub.medicare_withholding_ytd_cents, "Medicare withholding YTD"},
           std::pair{paystub.california_sdi_withholding_ytd_cents, "California SDI withholding YTD"},
       }) {
    validate_nonnegative(value, field);
  }
  if (paystub.federal_taxable_wages_ytd_cents < paystub.current_period_regular_wages_cents +
                                                      paystub.current_period_bonus_wages_cents ||
      paystub.california_taxable_wages_ytd_cents < paystub.current_period_regular_wages_cents +
                                                         paystub.current_period_bonus_wages_cents ||
      paystub.federal_withholding_ytd_cents < paystub.current_period_federal_withholding_cents ||
      paystub.california_withholding_ytd_cents < paystub.current_period_california_withholding_cents) {
    throw ValidationError("paystub current-period values cannot exceed YTD values");
  }
}

void validate(const InvestmentSummary& investments)
{
  validate_nonnegative(investments.ordinary_dividends_cents, "ordinary dividends");
  validate_nonnegative(investments.qualified_dividends_cents, "qualified dividends");
  validate_nonnegative(investments.federal_withholding_cents, "federal investment withholding");
  validate_nonnegative(investments.california_withholding_cents, "California investment withholding");
  if (investments.qualified_dividends_cents > investments.ordinary_dividends_cents) {
    throw ValidationError("qualified dividends cannot exceed ordinary dividends",
                          "investments.qualified_dividends_cents", "exceeds_ordinary_dividends");
  }
}

void validate(const EstimatedPayment& payment)
{
  validate_nonnegative(payment.amount_cents, "estimated payment");
  if (payment.date && !is_valid_date(*payment.date)) {
    throw ValidationError("payment date must use YYYY-MM-DD");
  }
  if ((payment.amount_cents == 0) != !payment.date) {
    throw ValidationError("a payment date requires a positive payment amount");
  }
}

class Transaction {
public:
  explicit Transaction(sqlite3* database) : database_(database)
  {
    execute(database_, "BEGIN IMMEDIATE");
  }

  ~Transaction()
  {
    if (!committed_) {
      sqlite3_exec(database_, "ROLLBACK", nullptr, nullptr, nullptr);
    }
  }

  void commit()
  {
    execute(database_, "COMMIT");
    committed_ = true;
  }

private:
  sqlite3* database_;
  bool committed_{};
};

}  // namespace

struct InputStore::Connection {
  explicit Connection(const std::string& path) : sqlite(path) {}

  sqlite::Connection sqlite;
  sqlite3* database{sqlite.get()};
};

void validate(const Household& household)
{
  const auto valid_spouse = [](const Spouse& spouse, SpouseKey expected) {
    if (spouse.key != expected) {
      throw ValidationError("household spouse identity is invalid");
    }
    if (spouse.label.empty()) {
      throw ValidationError("spouse label cannot be empty");
    }
  };
  valid_spouse(household.spouse_1, SpouseKey::spouse_1);
  valid_spouse(household.spouse_2, SpouseKey::spouse_2);
}

void validate(const QuarterInput& quarter)
{
  if (quarter.quarter < 1 || quarter.quarter > 4) {
    throw ValidationError("quarter must be between 1 and 4");
  }
  if (quarter.spouse_1_paystub) {
    validate(*quarter.spouse_1_paystub);
  }
  if (quarter.spouse_2_paystub) {
    validate(*quarter.spouse_2_paystub);
  }
  if (quarter.investments) {
    validate(*quarter.investments);
  }
  validate(quarter.federal_payment);
  validate(quarter.california_payment);
}

InputStore::InputStore(const std::string& database_path) : connection_(std::make_unique<Connection>(database_path))
{
  Transaction transaction(connection_->database);
  execute(connection_->database,
          "CREATE TABLE IF NOT EXISTS schema_migrations (version INTEGER PRIMARY KEY CHECK (version > 0))");

  Statement version_statement(connection_->database, "SELECT COALESCE(MAX(version), 0) FROM schema_migrations");
  if (sqlite3_step(version_statement.get()) != SQLITE_ROW) {
    throw_sqlite_error(connection_->database, "read schema version");
  }
  const int version = sqlite3_column_int(version_statement.get(), 0);
  if (version > kSchemaVersion) {
    throw StorageError("database schema is newer than this application");
  }
  if (version == 0) {
    execute(connection_->database,
            "CREATE TABLE household ("
            "id INTEGER PRIMARY KEY CHECK (id = 1), "
            "tax_year INTEGER NOT NULL CHECK (tax_year = 2026), "
            "filing_status TEXT NOT NULL CHECK (filing_status = 'married_filing_jointly'), "
            "residency TEXT NOT NULL CHECK (residency = 'california_full_year'))");
    execute(connection_->database,
            "CREATE TABLE spouses ("
            "spouse_key TEXT PRIMARY KEY CHECK (spouse_key IN ('spouse_1', 'spouse_2')), "
            "household_id INTEGER NOT NULL REFERENCES household(id), "
            "label TEXT NOT NULL CHECK (length(label) > 0), "
            "age_65_or_older INTEGER NOT NULL CHECK (age_65_or_older IN (0, 1)), "
            "blind INTEGER NOT NULL CHECK (blind IN (0, 1)))");
    execute(connection_->database,
            "CREATE TABLE quarters (quarter INTEGER PRIMARY KEY CHECK (quarter BETWEEN 1 AND 4))");
    execute(connection_->database,
            "CREATE TABLE paystubs ("
            "quarter INTEGER NOT NULL REFERENCES quarters(quarter) ON DELETE CASCADE, "
            "spouse_key TEXT NOT NULL REFERENCES spouses(spouse_key), "
            "date TEXT NOT NULL, pay_frequency TEXT NOT NULL CHECK (pay_frequency IN ('weekly','biweekly','semimonthly','monthly')), "
            "current_period_regular_wages_cents INTEGER NOT NULL CHECK (current_period_regular_wages_cents >= 0), "
            "current_period_bonus_wages_cents INTEGER NOT NULL CHECK (current_period_bonus_wages_cents >= 0), "
            "current_period_federal_withholding_cents INTEGER NOT NULL CHECK (current_period_federal_withholding_cents >= 0), "
            "current_period_california_withholding_cents INTEGER NOT NULL CHECK (current_period_california_withholding_cents >= 0), "
            "federal_taxable_wages_ytd_cents INTEGER NOT NULL CHECK (federal_taxable_wages_ytd_cents >= 0), "
            "california_taxable_wages_ytd_cents INTEGER NOT NULL CHECK (california_taxable_wages_ytd_cents >= 0), "
            "federal_withholding_ytd_cents INTEGER NOT NULL CHECK (federal_withholding_ytd_cents >= 0), "
            "california_withholding_ytd_cents INTEGER NOT NULL CHECK (california_withholding_ytd_cents >= 0), "
            "social_security_withholding_ytd_cents INTEGER NOT NULL CHECK (social_security_withholding_ytd_cents >= 0), "
            "medicare_withholding_ytd_cents INTEGER NOT NULL CHECK (medicare_withholding_ytd_cents >= 0), "
            "california_sdi_withholding_ytd_cents INTEGER NOT NULL CHECK (california_sdi_withholding_ytd_cents >= 0), "
            "PRIMARY KEY (quarter, spouse_key))");
    execute(connection_->database,
            "CREATE TABLE investments ("
            "quarter INTEGER PRIMARY KEY REFERENCES quarters(quarter) ON DELETE CASCADE, "
            "ordinary_dividends_cents INTEGER NOT NULL CHECK (ordinary_dividends_cents >= 0), "
            "qualified_dividends_cents INTEGER NOT NULL CHECK (qualified_dividends_cents >= 0 AND qualified_dividends_cents <= ordinary_dividends_cents), "
            "short_term_gain_cents INTEGER NOT NULL, long_term_gain_cents INTEGER NOT NULL, "
            "federal_withholding_cents INTEGER NOT NULL CHECK (federal_withholding_cents >= 0), "
            "california_withholding_cents INTEGER NOT NULL CHECK (california_withholding_cents >= 0), notes TEXT)");
    execute(connection_->database,
            "CREATE TABLE estimated_payments ("
            "quarter INTEGER NOT NULL REFERENCES quarters(quarter) ON DELETE CASCADE, "
            "jurisdiction TEXT NOT NULL CHECK (jurisdiction IN ('federal','california')), "
            "amount_cents INTEGER NOT NULL CHECK (amount_cents >= 0), date TEXT, "
            "CHECK ((amount_cents = 0 AND date IS NULL) OR (amount_cents > 0 AND date IS NOT NULL)), "
            "PRIMARY KEY (quarter, jurisdiction))");
    execute(connection_->database,
            "INSERT INTO household VALUES (1, 2026, 'married_filing_jointly', 'california_full_year');"
            "INSERT INTO spouses VALUES ('spouse_1', 1, 'Spouse 1', 0, 0);"
            "INSERT INTO spouses VALUES ('spouse_2', 1, 'Spouse 2', 0, 0);"
            "INSERT INTO quarters VALUES (1), (2), (3), (4);"
            "INSERT INTO estimated_payments VALUES "
            "(1, 'federal', 0, NULL), (1, 'california', 0, NULL), "
            "(2, 'federal', 0, NULL), (2, 'california', 0, NULL), "
            "(3, 'federal', 0, NULL), (3, 'california', 0, NULL), "
            "(4, 'federal', 0, NULL), (4, 'california', 0, NULL);");
    execute(connection_->database, "INSERT INTO schema_migrations VALUES (1)");
  }
  transaction.commit();
}

InputStore::~InputStore() = default;

ValidationError::ValidationError(std::string message, std::string path, std::string code)
    : std::runtime_error(std::move(message)), path_(std::move(path)), code_(std::move(code)) {}

const std::string& ValidationError::path() const { return path_; }
const std::string& ValidationError::code() const { return code_; }

int InputStore::schema_version() const
{
  Statement statement(connection_->database, "SELECT MAX(version) FROM schema_migrations");
  if (sqlite3_step(statement.get()) != SQLITE_ROW) {
    throw_sqlite_error(connection_->database, "read schema version");
  }
  return sqlite3_column_int(statement.get(), 0);
}

Household InputStore::load_household() const
{
  Household household;
  Statement statement(connection_->database,
                      "SELECT spouse_key, label, age_65_or_older, blind FROM spouses ORDER BY spouse_key");
  int count = 0;
  int step_result = SQLITE_OK;
  while ((step_result = sqlite3_step(statement.get())) == SQLITE_ROW) {
    Spouse spouse{spouse_key_from_name(sqlite3_column_text(statement.get(), 0)),
                  reinterpret_cast<const char*>(sqlite3_column_text(statement.get(), 1)),
                  sqlite3_column_int(statement.get(), 2) != 0, sqlite3_column_int(statement.get(), 3) != 0};
    if (spouse.key == SpouseKey::spouse_1) {
      household.spouse_1 = std::move(spouse);
    } else {
      household.spouse_2 = std::move(spouse);
    }
    ++count;
  }
  if (step_result != SQLITE_DONE || count != 2) {
    throw StorageError("stored household is incomplete");
  }
  validate(household);
  return household;
}

void InputStore::replace_household(const Household& household)
{
  validate(household);
  Transaction transaction(connection_->database);
  Statement statement(connection_->database,
                      "UPDATE spouses SET label = ?, age_65_or_older = ?, blind = ? WHERE spouse_key = ?");
  for (const Spouse* spouse : {&household.spouse_1, &household.spouse_2}) {
    bind_text(statement.get(), 1, spouse->label);
    bind_integer(statement.get(), 2, spouse->age_65_or_older ? 1 : 0);
    bind_integer(statement.get(), 3, spouse->blind ? 1 : 0);
    bind_text(statement.get(), 4, std::string(spouse_key_name(spouse->key)));
    expect_done(connection_->database, statement.get(), "replace household");
    if (sqlite3_changes(connection_->database) != 1) {
      throw StorageError("stored spouse is missing");
    }
    sqlite3_reset(statement.get());
    sqlite3_clear_bindings(statement.get());
  }
  transaction.commit();
}

QuarterInput InputStore::load_quarter(int quarter) const
{
  if (quarter < 1 || quarter > 4) {
    throw ValidationError("quarter must be between 1 and 4");
  }
  QuarterInput input;
  input.quarter = quarter;
  {
    Statement statement(connection_->database,
                        "SELECT spouse_key, date, pay_frequency, current_period_regular_wages_cents, "
                        "current_period_bonus_wages_cents, current_period_federal_withholding_cents, "
                        "current_period_california_withholding_cents, federal_taxable_wages_ytd_cents, "
                        "california_taxable_wages_ytd_cents, federal_withholding_ytd_cents, "
                        "california_withholding_ytd_cents, social_security_withholding_ytd_cents, "
                        "medicare_withholding_ytd_cents, california_sdi_withholding_ytd_cents "
                        "FROM paystubs WHERE quarter = ?");
    bind_integer(statement.get(), 1, quarter);
    int step_result = SQLITE_OK;
    while ((step_result = sqlite3_step(statement.get())) == SQLITE_ROW) {
      PaystubSnapshot paystub{reinterpret_cast<const char*>(sqlite3_column_text(statement.get(), 1)),
                              reinterpret_cast<const char*>(sqlite3_column_text(statement.get(), 2))};
      Cents* fields[] = {&paystub.current_period_regular_wages_cents, &paystub.current_period_bonus_wages_cents,
                         &paystub.current_period_federal_withholding_cents, &paystub.current_period_california_withholding_cents,
                         &paystub.federal_taxable_wages_ytd_cents, &paystub.california_taxable_wages_ytd_cents,
                         &paystub.federal_withholding_ytd_cents, &paystub.california_withholding_ytd_cents,
                         &paystub.social_security_withholding_ytd_cents, &paystub.medicare_withholding_ytd_cents,
                         &paystub.california_sdi_withholding_ytd_cents};
      for (int index = 0; index < 11; ++index) {
        *fields[index] = sqlite3_column_int64(statement.get(), index + 3);
      }
      if (spouse_key_from_name(sqlite3_column_text(statement.get(), 0)) == SpouseKey::spouse_1) {
        input.spouse_1_paystub = std::move(paystub);
      } else {
        input.spouse_2_paystub = std::move(paystub);
      }
    }
    if (step_result != SQLITE_DONE) {
      throw_sqlite_error(connection_->database, "read paystubs");
    }
  }
  {
    Statement statement(connection_->database,
                        "SELECT ordinary_dividends_cents, qualified_dividends_cents, short_term_gain_cents, "
                        "long_term_gain_cents, federal_withholding_cents, california_withholding_cents, notes "
                        "FROM investments WHERE quarter = ?");
    bind_integer(statement.get(), 1, quarter);
    const int step_result = sqlite3_step(statement.get());
    if (step_result == SQLITE_ROW) {
      InvestmentSummary investments{sqlite3_column_int64(statement.get(), 0), sqlite3_column_int64(statement.get(), 1),
                                    sqlite3_column_int64(statement.get(), 2), sqlite3_column_int64(statement.get(), 3),
                                    sqlite3_column_int64(statement.get(), 4), sqlite3_column_int64(statement.get(), 5), std::nullopt};
      if (sqlite3_column_type(statement.get(), 6) != SQLITE_NULL) {
        investments.notes = reinterpret_cast<const char*>(sqlite3_column_text(statement.get(), 6));
      }
      input.investments = std::move(investments);
    } else if (step_result != SQLITE_DONE) {
      throw_sqlite_error(connection_->database, "read investments");
    }
  }
  {
    Statement statement(connection_->database,
                        "SELECT jurisdiction, amount_cents, date FROM estimated_payments WHERE quarter = ?");
    bind_integer(statement.get(), 1, quarter);
    int count = 0;
    int step_result = SQLITE_OK;
    while ((step_result = sqlite3_step(statement.get())) == SQLITE_ROW) {
      EstimatedPayment payment{sqlite3_column_int64(statement.get(), 1), std::nullopt};
      if (sqlite3_column_type(statement.get(), 2) != SQLITE_NULL) {
        payment.date = reinterpret_cast<const char*>(sqlite3_column_text(statement.get(), 2));
      }
      const std::string_view jurisdiction(reinterpret_cast<const char*>(sqlite3_column_text(statement.get(), 0)));
      if (jurisdiction == "federal") {
        input.federal_payment = std::move(payment);
      } else if (jurisdiction == "california") {
        input.california_payment = std::move(payment);
      } else {
        throw StorageError("stored payment jurisdiction is invalid");
      }
      ++count;
    }
    if (step_result != SQLITE_DONE || count != 2) {
      throw StorageError("stored payments are incomplete");
    }
  }
  validate(input);
  return input;
}

void InputStore::replace_quarter(const QuarterInput& quarter)
{
  validate(quarter);
  Transaction transaction(connection_->database);
  {
    Statement statement(connection_->database, "DELETE FROM paystubs WHERE quarter = ?");
    bind_integer(statement.get(), 1, quarter.quarter);
    expect_done(connection_->database, statement.get(), "clear paystubs");
  }
  {
    Statement statement(connection_->database, "DELETE FROM investments WHERE quarter = ?");
    bind_integer(statement.get(), 1, quarter.quarter);
    expect_done(connection_->database, statement.get(), "clear investments");
  }
  const auto insert_paystub = [this, &quarter](SpouseKey spouse_key, const std::optional<PaystubSnapshot>& paystub) {
    if (!paystub) return;
    Statement statement(connection_->database,
                        "INSERT INTO paystubs VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?) ");
    bind_integer(statement.get(), 1, quarter.quarter);
    bind_text(statement.get(), 2, std::string(spouse_key_name(spouse_key)));
    bind_text(statement.get(), 3, paystub->date);
    bind_text(statement.get(), 4, paystub->pay_frequency);
    const Cents values[] = {paystub->current_period_regular_wages_cents, paystub->current_period_bonus_wages_cents,
                            paystub->current_period_federal_withholding_cents, paystub->current_period_california_withholding_cents,
                            paystub->federal_taxable_wages_ytd_cents, paystub->california_taxable_wages_ytd_cents,
                            paystub->federal_withholding_ytd_cents, paystub->california_withholding_ytd_cents,
                            paystub->social_security_withholding_ytd_cents, paystub->medicare_withholding_ytd_cents,
                            paystub->california_sdi_withholding_ytd_cents};
    for (int index = 0; index < 11; ++index) bind_integer(statement.get(), index + 5, values[index]);
    expect_done(connection_->database, statement.get(), "insert paystub");
  };
  insert_paystub(SpouseKey::spouse_1, quarter.spouse_1_paystub);
  insert_paystub(SpouseKey::spouse_2, quarter.spouse_2_paystub);
  if (quarter.investments) {
    Statement statement(connection_->database, "INSERT INTO investments VALUES (?, ?, ?, ?, ?, ?, ?, ?)");
    bind_integer(statement.get(), 1, quarter.quarter);
    bind_integer(statement.get(), 2, quarter.investments->ordinary_dividends_cents);
    bind_integer(statement.get(), 3, quarter.investments->qualified_dividends_cents);
    bind_integer(statement.get(), 4, quarter.investments->short_term_gain_cents);
    bind_integer(statement.get(), 5, quarter.investments->long_term_gain_cents);
    bind_integer(statement.get(), 6, quarter.investments->federal_withholding_cents);
    bind_integer(statement.get(), 7, quarter.investments->california_withholding_cents);
    bind_optional_text(statement.get(), 8, quarter.investments->notes);
    expect_done(connection_->database, statement.get(), "insert investments");
  }
  {
    Statement statement(connection_->database,
                        "UPDATE estimated_payments SET amount_cents = ?, date = ? WHERE quarter = ? AND jurisdiction = ?");
    const auto save_payment = [this, &statement, &quarter](std::string_view jurisdiction, const EstimatedPayment& payment) {
      bind_integer(statement.get(), 1, payment.amount_cents);
      bind_optional_text(statement.get(), 2, payment.date);
      bind_integer(statement.get(), 3, quarter.quarter);
      bind_text(statement.get(), 4, std::string(jurisdiction));
      expect_done(connection_->database, statement.get(), "replace payment");
      if (sqlite3_changes(connection_->database) != 1) throw StorageError("stored payment is missing");
      sqlite3_reset(statement.get());
      sqlite3_clear_bindings(statement.get());
    };
    save_payment("federal", quarter.federal_payment);
    save_payment("california", quarter.california_payment);
  }
  transaction.commit();
}

}  // namespace estimated_taxes
