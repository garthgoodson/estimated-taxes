#pragma once

#include "estimated_taxes/tax_calculation.hpp"

#include <optional>

namespace estimated_taxes {

enum class CalculationStatus { available, insufficient_information };
enum class RecommendationStatus { payment_recommended, catch_up_recommended, no_payment_currently_needed };
enum class DueDateStatus { upcoming, due_today, past_due };
enum class ValidationSeverity { blocking, caution, informational };

struct ValidationOutcome {
  ValidationSeverity severity;
  std::optional<Jurisdiction> jurisdiction;
  std::string code;
  std::string message;
  bool operator==(const ValidationOutcome&) const = default;
};

struct FutureInstallmentOutlook {
  int quarter{};
  Cents cumulative_target_cents{};
  Cents recommended_payment_cents{};
  bool operator==(const FutureInstallmentOutlook&) const = default;
};

struct PaymentRecommendation {
  CalculationStatus calculation_status{CalculationStatus::available};
  std::optional<RecommendationStatus> recommendation_status;
  std::optional<DueDateStatus> due_date_status;
  Cents projected_overpayment_cents{};
  bool projected_overpayment{};
  std::optional<Cents> annual_target_cents;
  std::optional<Cents> cumulative_target_cents;
  std::optional<Cents> payments_credited_cents;
  std::optional<Cents> scheduled_installment_cents;
  std::optional<Cents> scheduled_recommendation_cents;
  std::optional<Cents> catch_up_recommendation_cents;
  std::optional<Cents> recommended_payment_cents;
  std::optional<Cents> remaining_before_recommendation_cents;
  std::optional<Cents> remaining_after_recommendation_cents;
  int current_quarter{};
  std::optional<std::string> due_date;
  std::vector<FutureInstallmentOutlook> future_outlook;
  std::int64_t rule_revision_id{};
};

struct RecommendationResult {
  PaymentRecommendation federal;
  PaymentRecommendation california;
  std::vector<ValidationOutcome> validation;
};

struct CurrentResult {
  Household household;
  std::string as_of_date;
  AnnualProjection projection;
  TaxCalculation taxes;
  RecommendationResult recommendations;
};

class RecommendationError : public std::runtime_error { public: using std::runtime_error::runtime_error; };

[[nodiscard]] RecommendationResult recommend_payments(const TaxYearInputs& inputs, const std::string& as_of_date,
                                                       const TaxCalculation& taxes, const ActiveRules& rules,
                                                       std::vector<ValidationOutcome> validation = {});
[[nodiscard]] CurrentResult compose_current_result(const Household& household, const TaxYearInputs& inputs,
                                                    const std::string& as_of_date, const ActiveRules& rules,
                                                    std::vector<ValidationOutcome> validation = {});

}  // namespace estimated_taxes
