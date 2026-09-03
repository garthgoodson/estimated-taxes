#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>

namespace estimated_taxes {

using Cents = std::int64_t;

enum class SpouseKey {
  spouse_1,
  spouse_2,
};

struct Spouse {
  SpouseKey key;
  std::string label;
  bool age_65_or_older{};
  bool blind{};

  bool operator==(const Spouse&) const = default;
};

struct Household {
  Spouse spouse_1{SpouseKey::spouse_1, "Spouse 1"};
  Spouse spouse_2{SpouseKey::spouse_2, "Spouse 2"};

  bool operator==(const Household&) const = default;
};

struct PaystubSnapshot {
  std::string date;
  std::string pay_frequency;
  Cents current_period_regular_wages_cents{};
  Cents current_period_bonus_wages_cents{};
  Cents current_period_federal_withholding_cents{};
  Cents current_period_california_withholding_cents{};
  Cents federal_taxable_wages_ytd_cents{};
  Cents california_taxable_wages_ytd_cents{};
  Cents federal_withholding_ytd_cents{};
  Cents california_withholding_ytd_cents{};
  Cents social_security_withholding_ytd_cents{};
  Cents medicare_withholding_ytd_cents{};
  Cents california_sdi_withholding_ytd_cents{};

  bool operator==(const PaystubSnapshot&) const = default;
};

struct InvestmentSummary {
  Cents ordinary_dividends_cents{};
  Cents qualified_dividends_cents{};
  Cents short_term_gain_cents{};
  Cents long_term_gain_cents{};
  Cents federal_withholding_cents{};
  Cents california_withholding_cents{};
  std::optional<std::string> notes;

  bool operator==(const InvestmentSummary&) const = default;
};

struct EstimatedPayment {
  Cents amount_cents{};
  std::optional<std::string> date;

  bool operator==(const EstimatedPayment&) const = default;
};

struct QuarterInput {
  int quarter{};
  std::optional<PaystubSnapshot> spouse_1_paystub;
  std::optional<PaystubSnapshot> spouse_2_paystub;
  std::optional<InvestmentSummary> investments;
  EstimatedPayment federal_payment;
  EstimatedPayment california_payment;

  bool operator==(const QuarterInput&) const = default;
};

class ValidationError : public std::runtime_error {
public:
  explicit ValidationError(std::string message, std::string path = "request",
                           std::string code = "invalid_value");

  [[nodiscard]] const std::string& path() const;
  [[nodiscard]] const std::string& code() const;

private:
  std::string path_;
  std::string code_;
};

class StorageError : public std::runtime_error {
public:
  using std::runtime_error::runtime_error;
};

void validate(const Household& household);
void validate(const QuarterInput& quarter);

class InputStore {
public:
  explicit InputStore(const std::string& database_path);
  ~InputStore();

  InputStore(const InputStore&) = delete;
  InputStore& operator=(const InputStore&) = delete;
  InputStore(InputStore&&) = delete;
  InputStore& operator=(InputStore&&) = delete;

  [[nodiscard]] int schema_version() const;
  [[nodiscard]] Household load_household() const;
  void replace_household(const Household& household);
  [[nodiscard]] QuarterInput load_quarter(int quarter) const;
  void replace_quarter(const QuarterInput& quarter);

private:
  struct Connection;
  std::unique_ptr<Connection> connection_;
};

}  // namespace estimated_taxes
