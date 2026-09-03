#pragma once

#include "estimated_taxes/input_store.hpp"

#include <array>
#include <optional>
#include <string>
#include <vector>

namespace estimated_taxes {

struct TaxYearInputs {
  std::array<QuarterInput, 4> quarters;
};

enum class WarningSeverity {
  caution,
  information,
};

struct ProjectionWarning {
  WarningSeverity severity;
  std::string code;
  std::string path;
  std::string message;

  bool operator==(const ProjectionWarning&) const = default;
};

struct ProjectionAmounts {
  Cents actual_cents{};
  Cents projected_remaining_cents{};
  Cents projected_annual_cents{};

  bool operator==(const ProjectionAmounts&) const = default;
};

struct SpouseProjection {
  SpouseKey spouse;
  std::optional<int> authoritative_quarter;
  int remaining_pay_periods{};
  ProjectionAmounts federal_wages;
  ProjectionAmounts california_wages;
  ProjectionAmounts federal_withholding;
  ProjectionAmounts california_withholding;

  bool operator==(const SpouseProjection&) const = default;
};

struct InvestmentProjection {
  Cents ordinary_dividends_cents{};
  Cents qualified_dividends_cents{};
  Cents short_term_gain_cents{};
  Cents long_term_gain_cents{};
  Cents federal_withholding_cents{};
  Cents california_withholding_cents{};

  bool operator==(const InvestmentProjection&) const = default;
};

struct AnnualProjection {
  std::array<SpouseProjection, 2> spouses;
  ProjectionAmounts federal_wages;
  ProjectionAmounts california_wages;
  ProjectionAmounts federal_withholding;
  ProjectionAmounts california_withholding;
  InvestmentProjection investments;
  std::vector<ProjectionWarning> warnings;
};

class ProjectionError : public std::runtime_error {
public:
  using std::runtime_error::runtime_error;
};

[[nodiscard]] AnnualProjection project_annual(const TaxYearInputs& inputs, const std::string& as_of_date);

}  // namespace estimated_taxes
