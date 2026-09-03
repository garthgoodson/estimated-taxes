#pragma once

#include "estimated_taxes/projection.hpp"
#include "estimated_taxes/tax_rules.hpp"

namespace estimated_taxes {

struct CapitalNetting {
  Cents ordinary_gain_cents{};
  Cents preferential_gain_cents{};
  Cents deductible_loss_cents{};
  Cents unused_loss_cents{};
};

struct TaxCalculationDetails {
  Cents supported_agi_cents{};
  Cents deduction_cents{};
  Cents taxable_income_cents{};
  Cents preferential_pool_cents{};
  Cents ordinary_taxable_income_cents{};
  Cents ordinary_tax_cents{};
  Cents preferential_tax_cents{};
  Cents ordinary_tax_on_all_income_cents{};
  Cents regular_tax_cents{};
  Cents exemption_credit_cents{};
  Cents tax_before_exemption_cents{};
  Cents tax_after_exemption_cents{};
  Cents niit_cents{};
  Cents additional_medicare_cents{};
  Cents behavioral_health_services_cents{};
  Cents annual_liability_cents{};
  Cents projected_withholding_cents{};
  Cents amount_requiring_estimated_payments_cents{};
};

struct TaxResult {
  TaxCalculationDetails details;
  CapitalNetting capital_netting;
  int effective_rate_ppm{};
  std::int64_t rule_revision_id{};
  std::vector<ProjectionWarning> warnings;
};

struct TaxCalculation {
  TaxResult federal;
  TaxResult california;
};

class CalculationError : public std::runtime_error { public: using std::runtime_error::runtime_error; };

[[nodiscard]] Cents multiply_rate(Cents amount_cents, int rate_ppm);
[[nodiscard]] Cents progressive_tax(Cents taxable_income_cents, const std::vector<Bracket>& brackets);
[[nodiscard]] CapitalNetting net_capital_gains(Cents short_term_cents, Cents long_term_cents, Cents loss_limit_cents);
[[nodiscard]] TaxCalculation calculate_tax(const AnnualProjection& projection, const Household& household,
                                           const ActiveRules& rules);

}  // namespace estimated_taxes
