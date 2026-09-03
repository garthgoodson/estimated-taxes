#include "estimated_taxes/input_store.hpp"
#include "estimated_taxes/sqlite.hpp"

#include <sqlite3.h>

#include <array>
#include <chrono>
#include <memory>
#include <utility>

namespace estimated_taxes {
namespace {

constexpr int kSchemaVersion = 3;

bool valid_date(const std::string& value, int first_year = 2026, int last_year = 2026)
{
  if (value.size() != 10 || value[4] != '-' || value[7] != '-') return false;
  for (const int index : {0, 1, 2, 3, 5, 6, 8, 9}) {
    if (value[index] < '0' || value[index] > '9') return false;
  }
  const int year = std::stoi(value.substr(0, 4));
  const unsigned month = static_cast<unsigned>(std::stoi(value.substr(5, 2)));
  const unsigned day = static_cast<unsigned>(std::stoi(value.substr(8, 2)));
  return year >= first_year && year <= last_year &&
         std::chrono::year_month_day{std::chrono::year{year}, std::chrono::month{month}, std::chrono::day{day}}.ok();
}

void require_nonnegative(Cents value, std::string_view field)
{
  if (value < 0) throw ValidationError(std::string(field) + " cannot be negative");
}

void validate_paystub(const PaystubSnapshot& paystub)
{
  if (!valid_date(paystub.date)) throw ValidationError("paystub date must use YYYY-MM-DD");
  if (paystub.pay_frequency != "weekly" && paystub.pay_frequency != "biweekly" &&
      paystub.pay_frequency != "semimonthly" && paystub.pay_frequency != "monthly") {
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
           std::pair{paystub.california_sdi_withholding_ytd_cents, "California SDI withholding YTD"}}) {
    require_nonnegative(value, field);
  }
  if (paystub.federal_taxable_wages_ytd_cents < paystub.current_period_regular_wages_cents + paystub.current_period_bonus_wages_cents ||
      paystub.california_taxable_wages_ytd_cents < paystub.current_period_regular_wages_cents + paystub.current_period_bonus_wages_cents ||
      paystub.federal_withholding_ytd_cents < paystub.current_period_federal_withholding_cents ||
      paystub.california_withholding_ytd_cents < paystub.current_period_california_withholding_cents) {
    throw ValidationError("paystub current-period values cannot exceed YTD values");
  }
}

void validate_investments(const InvestmentSummary& value)
{
  require_nonnegative(value.ordinary_dividends_cents, "ordinary dividends");
  require_nonnegative(value.qualified_dividends_cents, "qualified dividends");
  require_nonnegative(value.federal_withholding_cents, "federal investment withholding");
  require_nonnegative(value.california_withholding_cents, "California investment withholding");
  if (value.qualified_dividends_cents > value.ordinary_dividends_cents) {
    throw ValidationError("qualified dividends cannot exceed ordinary dividends",
                          "investments.qualified_dividends_cents", "exceeds_ordinary_dividends");
  }
}

void validate_payment(const EstimatedPayment& value)
{
  require_nonnegative(value.amount_cents, "estimated payment");
  if (value.date && !valid_date(*value.date)) throw ValidationError("payment date must use YYYY-MM-DD");
  if ((value.amount_cents == 0) != !value.date) throw ValidationError("a payment date requires a positive payment amount");
}

const char* spouse_name(SpouseKey key) { return key == SpouseKey::spouse_1 ? "spouse_1" : "spouse_2"; }

SpouseKey spouse_key(const std::string& value)
{
  if (value == "spouse_1") return SpouseKey::spouse_1;
  if (value == "spouse_2") return SpouseKey::spouse_2;
  throw StorageError("stored spouse key is invalid");
}

void bind_payment(sqlite::Statement& statement, int quarter, const char* jurisdiction, const EstimatedPayment& payment)
{
  statement.bind_integer(1, payment.amount_cents);
  if (payment.date) statement.bind_text(2, *payment.date); else statement.bind_null(2);
  statement.bind_integer(3, quarter);
  statement.bind_text(4, jurisdiction);
  statement.step_done("replace payment");
  statement.reset();
}

}  // namespace

struct InputStore::Connection {
  explicit Connection(const std::string& path) : sqlite(path) {}
  sqlite::Connection sqlite;
};

ValidationError::ValidationError(std::string message, std::string path, std::string code)
    : std::runtime_error(std::move(message)), path_(std::move(path)), code_(std::move(code)) {}
const std::string& ValidationError::path() const { return path_; }
const std::string& ValidationError::code() const { return code_; }

void validate(const Household& household)
{
  const auto validate_spouse = [](const Spouse& spouse, SpouseKey expected) {
    if (spouse.key != expected) throw ValidationError("household spouse identity is invalid");
    if (spouse.label.empty()) throw ValidationError("spouse label cannot be empty");
  };
  validate_spouse(household.spouse_1, SpouseKey::spouse_1);
  validate_spouse(household.spouse_2, SpouseKey::spouse_2);
}

void validate(const QuarterInput& quarter)
{
  if (quarter.quarter < 1 || quarter.quarter > 4) throw ValidationError("quarter must be between 1 and 4");
  if (quarter.spouse_1_paystub) validate_paystub(*quarter.spouse_1_paystub);
  if (quarter.spouse_2_paystub) validate_paystub(*quarter.spouse_2_paystub);
  if (quarter.investments) validate_investments(*quarter.investments);
  validate_payment(quarter.federal_payment);
  validate_payment(quarter.california_payment);
}

InputStore::InputStore(const std::string& path) : connection_(std::make_unique<Connection>(path))
{
  auto& database = connection_->sqlite;
  sqlite::Transaction transaction(database);
  database.execute("CREATE TABLE IF NOT EXISTS schema_migrations (version INTEGER PRIMARY KEY CHECK (version > 0))");
  sqlite::Statement version(database.get(), "SELECT COALESCE(MAX(version), 0) FROM schema_migrations");
  if (!version.step_row()) throw StorageError("read schema version failed");
  if (version.integer(0) > kSchemaVersion) throw StorageError("database schema is newer than this application");
  if (version.integer(0) == 0) {
    database.execute("CREATE TABLE household (id INTEGER PRIMARY KEY CHECK (id = 1), tax_year INTEGER NOT NULL CHECK (tax_year = 2026), filing_status TEXT NOT NULL CHECK (filing_status = 'married_filing_jointly'), residency TEXT NOT NULL CHECK (residency = 'california_full_year'));"
                     "CREATE TABLE spouses (spouse_key TEXT PRIMARY KEY CHECK (spouse_key IN ('spouse_1', 'spouse_2')), household_id INTEGER NOT NULL REFERENCES household(id), label TEXT NOT NULL CHECK (length(label) > 0), age_65_or_older INTEGER NOT NULL CHECK (age_65_or_older IN (0, 1)), blind INTEGER NOT NULL CHECK (blind IN (0, 1)));"
                     "CREATE TABLE quarters (quarter INTEGER PRIMARY KEY CHECK (quarter BETWEEN 1 AND 4));"
                     "CREATE TABLE paystubs (quarter INTEGER NOT NULL REFERENCES quarters(quarter) ON DELETE CASCADE, spouse_key TEXT NOT NULL REFERENCES spouses(spouse_key), date TEXT NOT NULL, pay_frequency TEXT NOT NULL CHECK (pay_frequency IN ('weekly','biweekly','semimonthly','monthly')), current_period_regular_wages_cents INTEGER NOT NULL CHECK (current_period_regular_wages_cents >= 0), current_period_bonus_wages_cents INTEGER NOT NULL CHECK (current_period_bonus_wages_cents >= 0), current_period_federal_withholding_cents INTEGER NOT NULL CHECK (current_period_federal_withholding_cents >= 0), current_period_california_withholding_cents INTEGER NOT NULL CHECK (current_period_california_withholding_cents >= 0), federal_taxable_wages_ytd_cents INTEGER NOT NULL CHECK (federal_taxable_wages_ytd_cents >= 0), california_taxable_wages_ytd_cents INTEGER NOT NULL CHECK (california_taxable_wages_ytd_cents >= 0), federal_withholding_ytd_cents INTEGER NOT NULL CHECK (federal_withholding_ytd_cents >= 0), california_withholding_ytd_cents INTEGER NOT NULL CHECK (california_withholding_ytd_cents >= 0), social_security_withholding_ytd_cents INTEGER NOT NULL CHECK (social_security_withholding_ytd_cents >= 0), medicare_withholding_ytd_cents INTEGER NOT NULL CHECK (medicare_withholding_ytd_cents >= 0), california_sdi_withholding_ytd_cents INTEGER NOT NULL CHECK (california_sdi_withholding_ytd_cents >= 0), PRIMARY KEY (quarter, spouse_key));"
                     "CREATE TABLE investments (quarter INTEGER PRIMARY KEY REFERENCES quarters(quarter) ON DELETE CASCADE, ordinary_dividends_cents INTEGER NOT NULL CHECK (ordinary_dividends_cents >= 0), qualified_dividends_cents INTEGER NOT NULL CHECK (qualified_dividends_cents >= 0 AND qualified_dividends_cents <= ordinary_dividends_cents), short_term_gain_cents INTEGER NOT NULL, long_term_gain_cents INTEGER NOT NULL, federal_withholding_cents INTEGER NOT NULL CHECK (federal_withholding_cents >= 0), california_withholding_cents INTEGER NOT NULL CHECK (california_withholding_cents >= 0), notes TEXT);"
                     "CREATE TABLE estimated_payments (quarter INTEGER NOT NULL REFERENCES quarters(quarter) ON DELETE CASCADE, jurisdiction TEXT NOT NULL CHECK (jurisdiction IN ('federal','california')), amount_cents INTEGER NOT NULL CHECK (amount_cents >= 0), date TEXT, CHECK ((amount_cents = 0 AND date IS NULL) OR (amount_cents > 0 AND date IS NOT NULL)), PRIMARY KEY (quarter, jurisdiction));"
                     "INSERT INTO household VALUES (1, 2026, 'married_filing_jointly', 'california_full_year');"
                     "INSERT INTO spouses VALUES ('spouse_1', 1, 'Spouse 1', 0, 0); INSERT INTO spouses VALUES ('spouse_2', 1, 'Spouse 2', 0, 0);"
                     "INSERT INTO quarters VALUES (1), (2), (3), (4);"
                     "INSERT INTO estimated_payments VALUES (1, 'federal', 0, NULL), (1, 'california', 0, NULL), (2, 'federal', 0, NULL), (2, 'california', 0, NULL), (3, 'federal', 0, NULL), (3, 'california', 0, NULL), (4, 'federal', 0, NULL), (4, 'california', 0, NULL);"
                     "INSERT INTO schema_migrations VALUES (1)");
  }
  transaction.commit();
}

InputStore::~InputStore() = default;

int InputStore::schema_version() const
{
  sqlite::Statement statement(connection_->sqlite.get(), "SELECT MAX(version) FROM schema_migrations");
  if (!statement.step_row()) throw StorageError("read schema version failed");
  return static_cast<int>(statement.integer(0));
}

Household InputStore::load_household() const
{
  Household household;
  sqlite::Statement statement(connection_->sqlite.get(), "SELECT spouse_key, label, age_65_or_older, blind FROM spouses ORDER BY spouse_key");
  int count{};
  while (statement.step_row()) {
    const Spouse spouse{spouse_key(statement.required_text(0, "spouse key")), statement.required_text(1, "spouse label"),
                        statement.integer(2) != 0, statement.integer(3) != 0};
    if (spouse.key == SpouseKey::spouse_1) household.spouse_1 = spouse; else household.spouse_2 = spouse;
    ++count;
  }
  if (count != 2) throw StorageError("stored household is incomplete");
  validate(household);
  return household;
}

void InputStore::replace_household(const Household& household)
{
  validate(household);
  sqlite::Transaction transaction(connection_->sqlite);
  sqlite::Statement statement(connection_->sqlite.get(), "UPDATE spouses SET label=?, age_65_or_older=?, blind=? WHERE spouse_key=?");
  for (const Spouse* spouse : {&household.spouse_1, &household.spouse_2}) {
    statement.bind_text(1, spouse->label); statement.bind_integer(2, spouse->age_65_or_older); statement.bind_integer(3, spouse->blind); statement.bind_text(4, spouse_name(spouse->key));
    statement.step_done("replace household"); statement.reset();
  }
  transaction.commit();
}

QuarterInput InputStore::load_quarter(int quarter) const
{
  if (quarter < 1 || quarter > 4) throw ValidationError("quarter must be between 1 and 4");
  QuarterInput input; input.quarter = quarter;
  sqlite::Statement paystubs(connection_->sqlite.get(), "SELECT spouse_key,date,pay_frequency,current_period_regular_wages_cents,current_period_bonus_wages_cents,current_period_federal_withholding_cents,current_period_california_withholding_cents,federal_taxable_wages_ytd_cents,california_taxable_wages_ytd_cents,federal_withholding_ytd_cents,california_withholding_ytd_cents,social_security_withholding_ytd_cents,medicare_withholding_ytd_cents,california_sdi_withholding_ytd_cents FROM paystubs WHERE quarter=?");
  paystubs.bind_integer(1, quarter);
  while (paystubs.step_row()) {
    PaystubSnapshot value{paystubs.required_text(1, "paystub date"), paystubs.required_text(2, "pay frequency")};
    Cents* fields[] = {&value.current_period_regular_wages_cents,&value.current_period_bonus_wages_cents,&value.current_period_federal_withholding_cents,&value.current_period_california_withholding_cents,&value.federal_taxable_wages_ytd_cents,&value.california_taxable_wages_ytd_cents,&value.federal_withholding_ytd_cents,&value.california_withholding_ytd_cents,&value.social_security_withholding_ytd_cents,&value.medicare_withholding_ytd_cents,&value.california_sdi_withholding_ytd_cents};
    for (int index = 0; index < 11; ++index) *fields[index] = paystubs.integer(index + 3);
    if (spouse_key(paystubs.required_text(0, "paystub spouse key")) == SpouseKey::spouse_1) input.spouse_1_paystub = value; else input.spouse_2_paystub = value;
  }
  sqlite::Statement investments(connection_->sqlite.get(), "SELECT ordinary_dividends_cents,qualified_dividends_cents,short_term_gain_cents,long_term_gain_cents,federal_withholding_cents,california_withholding_cents,notes FROM investments WHERE quarter=?");
  investments.bind_integer(1, quarter);
  if (investments.step_row()) input.investments = InvestmentSummary{investments.integer(0), investments.integer(1), investments.integer(2), investments.integer(3), investments.integer(4), investments.integer(5), investments.optional_text(6)};
  sqlite::Statement payments(connection_->sqlite.get(), "SELECT jurisdiction,amount_cents,date FROM estimated_payments WHERE quarter=?");
  payments.bind_integer(1, quarter);
  int count{};
  while (payments.step_row()) {
    EstimatedPayment value{payments.integer(1), payments.optional_text(2)};
    const std::string jurisdiction = payments.required_text(0, "payment jurisdiction");
    if (jurisdiction == "federal") input.federal_payment = value; else if (jurisdiction == "california") input.california_payment = value; else throw StorageError("stored payment jurisdiction is invalid");
    ++count;
  }
  if (count != 2) throw StorageError("stored payments are incomplete");
  validate(input);
  return input;
}

void InputStore::replace_quarter(const QuarterInput& quarter)
{
  validate(quarter);
  sqlite::Transaction transaction(connection_->sqlite);
  for (const char* table : {"paystubs", "investments"}) {
    sqlite::Statement clear(connection_->sqlite.get(), (std::string("DELETE FROM ") + table + " WHERE quarter=?").c_str());
    clear.bind_integer(1, quarter.quarter); clear.step_done("clear quarter inputs");
  }
  sqlite::Statement insert(connection_->sqlite.get(), "INSERT INTO paystubs VALUES (?,?,?,?,?,?,?,?,?,?,?,?,?,?,?)");
  for (const auto [key, value] : {std::pair{SpouseKey::spouse_1, &quarter.spouse_1_paystub}, std::pair{SpouseKey::spouse_2, &quarter.spouse_2_paystub}}) {
    if (!*value) continue;
    insert.bind_integer(1, quarter.quarter); insert.bind_text(2, spouse_name(key)); insert.bind_text(3, (*value)->date); insert.bind_text(4, (*value)->pay_frequency);
    const Cents values[] = {(*value)->current_period_regular_wages_cents,(*value)->current_period_bonus_wages_cents,(*value)->current_period_federal_withholding_cents,(*value)->current_period_california_withholding_cents,(*value)->federal_taxable_wages_ytd_cents,(*value)->california_taxable_wages_ytd_cents,(*value)->federal_withholding_ytd_cents,(*value)->california_withholding_ytd_cents,(*value)->social_security_withholding_ytd_cents,(*value)->medicare_withholding_ytd_cents,(*value)->california_sdi_withholding_ytd_cents};
    for (int index = 0; index < 11; ++index) insert.bind_integer(index + 5, values[index]);
    insert.step_done("insert paystub"); insert.reset();
  }
  if (quarter.investments) {
    sqlite::Statement insert_investment(connection_->sqlite.get(), "INSERT INTO investments VALUES (?,?,?,?,?,?,?,?)");
    insert_investment.bind_integer(1, quarter.quarter); const auto& value = *quarter.investments;
    const Cents amounts[] = {value.ordinary_dividends_cents, value.qualified_dividends_cents,
                             value.short_term_gain_cents, value.long_term_gain_cents,
                             value.federal_withholding_cents, value.california_withholding_cents};
    for (int index = 0; index < 6; ++index) insert_investment.bind_integer(index + 2, amounts[index]);
    if (value.notes) insert_investment.bind_text(8, *value.notes); else insert_investment.bind_null(8);
    insert_investment.step_done("insert investments");
  }
  sqlite::Statement update(connection_->sqlite.get(), "UPDATE estimated_payments SET amount_cents=?,date=? WHERE quarter=? AND jurisdiction=?");
  bind_payment(update, quarter.quarter, "federal", quarter.federal_payment); bind_payment(update, quarter.quarter, "california", quarter.california_payment);
  transaction.commit();
}

}  // namespace estimated_taxes
