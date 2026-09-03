#include "estimated_taxes/projection.hpp"
#include "estimated_taxes/money.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <limits>
#include <string_view>

namespace estimated_taxes {
namespace {

using Days = std::chrono::sys_days;

struct Date {
  Days value;
  int quarter;
  int day_of_year;
};

struct SelectedPaystub {
  const PaystubSnapshot* paystub;
  int quarter;
  Date date;
};

[[nodiscard]] Cents checked_add(Cents left, Cents right)
{
  if ((right > 0 && left > std::numeric_limits<Cents>::max() - right) ||
      (right < 0 && left < std::numeric_limits<Cents>::min() - right)) {
    throw ProjectionError("monetary projection overflow");
  }
  return left + right;
}

[[nodiscard]] Cents checked_multiply(Cents left, int right)
{
  if (right != 0 && (left > std::numeric_limits<Cents>::max() / right ||
                     left < std::numeric_limits<Cents>::min() / right)) {
    throw ProjectionError("monetary projection overflow");
  }
  return left * right;
}

[[nodiscard]] Date parse_2026_date(const std::string& text, std::string_view field)
{
  if (text.size() != 10 || text[4] != '-' || text[7] != '-') {
    throw ProjectionError(std::string(field) + " must use YYYY-MM-DD");
  }
  for (const int index : {0, 1, 2, 3, 5, 6, 8, 9}) {
    if (text[index] < '0' || text[index] > '9') {
      throw ProjectionError(std::string(field) + " must use YYYY-MM-DD");
    }
  }
  const int year = std::stoi(text.substr(0, 4));
  const unsigned month = static_cast<unsigned>(std::stoi(text.substr(5, 2)));
  const unsigned day = static_cast<unsigned>(std::stoi(text.substr(8, 2)));
  const std::chrono::year_month_day calendar_date{
      std::chrono::year{year}, std::chrono::month{month}, std::chrono::day{day}};
  if (year != 2026 || !calendar_date.ok()) {
    throw ProjectionError(std::string(field) + " must be a valid 2026 date");
  }
  const Days first_day{std::chrono::year{2026} / std::chrono::January / 1};
  const Days value{calendar_date};
  const int day_of_year = static_cast<int>((value - first_day).count()) + 1;
  return Date{value, static_cast<int>((month - 1U) / 3U + 1U), day_of_year};
}

[[nodiscard]] int periods_per_year(const std::string& frequency)
{
  if (frequency == "weekly") return 52;
  if (frequency == "biweekly") return 26;
  if (frequency == "semimonthly") return 24;
  if (frequency == "monthly") return 12;
  throw ProjectionError("pay frequency is invalid");
}

[[nodiscard]] int stale_after_days(const std::string& frequency)
{
  if (frequency == "weekly") return 14;
  if (frequency == "biweekly") return 28;
  if (frequency == "semimonthly") return 31;
  if (frequency == "monthly") return 62;
  throw ProjectionError("pay frequency is invalid");
}

[[nodiscard]] int remaining_periods(const Date& paystub_date, const std::string& frequency)
{
  const int periods = periods_per_year(frequency);
  constexpr int days_in_2026 = 365;
  const int completed = (paystub_date.day_of_year * periods + days_in_2026 - 1) / days_in_2026;
  return std::max(0, periods - completed);
}

void add_warning(std::vector<ProjectionWarning>& warnings, WarningSeverity severity, std::string code,
                 std::string path, std::string message)
{
  warnings.push_back(ProjectionWarning{severity, std::move(code), std::move(path), std::move(message)});
}

[[nodiscard]] const std::optional<PaystubSnapshot>& paystub_for(const QuarterInput& quarter, SpouseKey spouse)
{
  return spouse == SpouseKey::spouse_1 ? quarter.spouse_1_paystub : quarter.spouse_2_paystub;
}

void validate_inputs(const TaxYearInputs& inputs, const Date& as_of)
{
  for (int index = 0; index < 4; ++index) {
    const QuarterInput& quarter = inputs.quarters[index];
    if (quarter.quarter != index + 1) {
      throw ProjectionError("tax-year inputs must contain Q1 through Q4 in order");
    }
    validate(quarter);
    const bool has_facts = quarter.spouse_1_paystub || quarter.spouse_2_paystub || quarter.investments ||
                           quarter.federal_payment.amount_cents != 0 || quarter.california_payment.amount_cents != 0;
    if (quarter.quarter > as_of.quarter && has_facts) {
      throw ProjectionError("future quarter contains facts");
    }
    for (const SpouseKey spouse : {SpouseKey::spouse_1, SpouseKey::spouse_2}) {
      if (const auto& paystub = paystub_for(quarter, spouse)) {
        if (parse_2026_date(paystub->date, "paystub date").value > as_of.value) {
          throw ProjectionError("paystub date is after the as-of date");
        }
      }
    }
  }
}

[[nodiscard]] std::optional<SelectedPaystub> select_paystub(const TaxYearInputs& inputs, SpouseKey spouse,
                                                             const Date& as_of,
                                                             std::vector<ProjectionWarning>& warnings)
{
  std::optional<SelectedPaystub> selected;
  for (int index = 0; index < 4; ++index) {
    const auto& paystub = paystub_for(inputs.quarters[index], spouse);
    if (!paystub) continue;
    const Date date = parse_2026_date(paystub->date, "paystub date");
    if (date.value > as_of.value) continue;
    if (!selected || date.value > selected->date.value ||
        (date.value == selected->date.value && index + 1 > selected->quarter)) {
      if (selected && date.value == selected->date.value) {
        add_warning(warnings, WarningSeverity::caution, "duplicate_paystub_date",
                    std::string("paystubs.") + (spouse == SpouseKey::spouse_1 ? "spouse_1" : "spouse_2"),
                    "Later quarter paystub takes precedence for a duplicate date.");
      }
      selected = SelectedPaystub{&*paystub, index + 1, date};
    }
  }
  return selected;
}

[[nodiscard]] const PaystubSnapshot* earlier_regular_paystub(const TaxYearInputs& inputs, SpouseKey spouse,
                                                              const SelectedPaystub& authoritative)
{
  const PaystubSnapshot* selected = nullptr;
  Days selected_date{};
  int selected_quarter = 0;
  for (int index = 0; index < 4; ++index) {
    const auto& candidate = paystub_for(inputs.quarters[index], spouse);
    if (!candidate || candidate->current_period_bonus_wages_cents != 0) continue;
    const Date date = parse_2026_date(candidate->date, "paystub date");
    if (date.value >= authoritative.date.value) continue;
    if (!selected || date.value > selected_date || (date.value == selected_date && index + 1 > selected_quarter)) {
      selected = &*candidate;
      selected_date = date.value;
      selected_quarter = index + 1;
    }
  }
  return selected;
}

[[nodiscard]] Cents repeated_withholding(const TaxYearInputs& inputs, SpouseKey spouse,
                                         const SelectedPaystub& authoritative, bool federal,
                                         std::vector<ProjectionWarning>& warnings)
{
  const PaystubSnapshot& paystub = *authoritative.paystub;
  const Cents current = federal ? paystub.current_period_federal_withholding_cents
                                : paystub.current_period_california_withholding_cents;
  const Cents total_wages = checked_add(paystub.current_period_regular_wages_cents,
                                        paystub.current_period_bonus_wages_cents);
  const std::string path = std::string("paystubs.") + (spouse == SpouseKey::spouse_1 ? "spouse_1" : "spouse_2");
  if (total_wages == 0) {
    add_warning(warnings, WarningSeverity::caution, "bonus_withholding_zero_wages", path,
                "Withholding could not be projected because current-period wages are zero.");
    return 0;
  }
  if (paystub.current_period_bonus_wages_cents == 0) return current;
  if (const PaystubSnapshot* regular = earlier_regular_paystub(inputs, spouse, authoritative)) {
    return federal ? regular->current_period_federal_withholding_cents
                   : regular->current_period_california_withholding_cents;
  }
  add_warning(warnings, WarningSeverity::caution, "bonus_withholding_proportional_allocation", path,
              "Current withholding was proportionally allocated between regular wages and bonus wages.");
  if (paystub.current_period_regular_wages_cents == 0) return 0;
  if (current > std::numeric_limits<Cents>::max() / paystub.current_period_regular_wages_cents) {
    throw ProjectionError("monetary projection overflow");
  }
  return divide_round_nearest(current * paystub.current_period_regular_wages_cents, total_wages);
}

[[nodiscard]] ProjectionAmounts amounts(Cents actual, Cents pattern, int periods)
{
  const Cents remaining = checked_multiply(pattern, periods);
  return ProjectionAmounts{actual, remaining, checked_add(actual, remaining)};
}

[[nodiscard]] SpouseProjection project_spouse(const TaxYearInputs& inputs, SpouseKey spouse, const Date& as_of,
                                               std::vector<ProjectionWarning>& warnings)
{
  SpouseProjection projection;
  projection.spouse = spouse;
  const auto authoritative = select_paystub(inputs, spouse, as_of, warnings);
  const std::string key = spouse == SpouseKey::spouse_1 ? "spouse_1" : "spouse_2";
  if (!authoritative) {
    bool earlier_paystub = false;
    for (int index = 0; index < as_of.quarter - 1; ++index) {
      earlier_paystub = earlier_paystub || paystub_for(inputs.quarters[index], spouse).has_value();
    }
    if (earlier_paystub) {
      add_warning(warnings, WarningSeverity::caution, "missing_current_quarter_paystub", "paystubs." + key,
                  "No paystub was entered for the current quarter.");
    }
    return projection;
  }

  const PaystubSnapshot& paystub = *authoritative->paystub;
  projection.authoritative_quarter = authoritative->quarter;
  if (authoritative->quarter < as_of.quarter) {
    add_warning(warnings, WarningSeverity::caution, "missing_current_quarter_paystub", "paystubs." + key,
                "No paystub was entered for the current quarter.");
  }
  projection.remaining_pay_periods = remaining_periods(authoritative->date, paystub.pay_frequency);
  const int age_days = static_cast<int>((as_of.value - authoritative->date.value).count());
  if (age_days > stale_after_days(paystub.pay_frequency)) {
    add_warning(warnings, WarningSeverity::caution, "stale_paystub", "paystubs." + key,
                "The authoritative paystub is older than two nominal pay periods.");
  }
  add_warning(warnings, WarningSeverity::information, "pay_period_approximation", "paystubs." + key,
              "Remaining pay periods use the supported nominal annual-period approximation.");

  projection.federal_wages = amounts(paystub.federal_taxable_wages_ytd_cents,
                                     paystub.current_period_regular_wages_cents, projection.remaining_pay_periods);
  projection.california_wages = amounts(paystub.california_taxable_wages_ytd_cents,
                                        paystub.current_period_regular_wages_cents, projection.remaining_pay_periods);
  projection.federal_withholding = amounts(paystub.federal_withholding_ytd_cents,
      repeated_withholding(inputs, spouse, *authoritative, true, warnings), projection.remaining_pay_periods);
  projection.california_withholding = amounts(paystub.california_withholding_ytd_cents,
      repeated_withholding(inputs, spouse, *authoritative, false, warnings), projection.remaining_pay_periods);
  return projection;
}

[[nodiscard]] ProjectionAmounts sum_amounts(const std::array<SpouseProjection, 2>& spouses,
                                            ProjectionAmounts SpouseProjection::* member)
{
  ProjectionAmounts total;
  for (const SpouseProjection& spouse : spouses) {
    const ProjectionAmounts& value = spouse.*member;
    total.actual_cents = checked_add(total.actual_cents, value.actual_cents);
    total.projected_remaining_cents = checked_add(total.projected_remaining_cents, value.projected_remaining_cents);
    total.projected_annual_cents = checked_add(total.projected_annual_cents, value.projected_annual_cents);
  }
  return total;
}

[[nodiscard]] InvestmentProjection project_investments(const TaxYearInputs& inputs, const Date& as_of,
                                                        std::vector<ProjectionWarning>& warnings)
{
  InvestmentProjection total;
  for (int index = 0; index < as_of.quarter; ++index) {
    const auto& investments = inputs.quarters[index].investments;
    if (!investments) {
      add_warning(warnings, WarningSeverity::caution, "missing_elapsed_investment_quarter",
                  "quarters." + std::to_string(index + 1) + ".investments",
                  "No investment summary was entered for this elapsed quarter.");
      continue;
    }
    total.ordinary_dividends_cents = checked_add(total.ordinary_dividends_cents, investments->ordinary_dividends_cents);
    total.qualified_dividends_cents = checked_add(total.qualified_dividends_cents, investments->qualified_dividends_cents);
    total.short_term_gain_cents = checked_add(total.short_term_gain_cents, investments->short_term_gain_cents);
    total.long_term_gain_cents = checked_add(total.long_term_gain_cents, investments->long_term_gain_cents);
    total.federal_withholding_cents = checked_add(total.federal_withholding_cents, investments->federal_withholding_cents);
    total.california_withholding_cents = checked_add(total.california_withholding_cents, investments->california_withholding_cents);
  }
  if (as_of.quarter < 4) {
    add_warning(warnings, WarningSeverity::information, "investment_income_not_projected",
                "investments", "Future investment income is assumed to be zero.");
  }
  return total;
}

}  // namespace

AnnualProjection project_annual(const TaxYearInputs& inputs, const std::string& as_of_date)
{
  const Date as_of = parse_2026_date(as_of_date, "as-of date");
  validate_inputs(inputs, as_of);

  AnnualProjection projection;
  projection.spouses[0] = project_spouse(inputs, SpouseKey::spouse_1, as_of, projection.warnings);
  projection.spouses[1] = project_spouse(inputs, SpouseKey::spouse_2, as_of, projection.warnings);
  projection.federal_wages = sum_amounts(projection.spouses, &SpouseProjection::federal_wages);
  projection.california_wages = sum_amounts(projection.spouses, &SpouseProjection::california_wages);
  projection.federal_withholding = sum_amounts(projection.spouses, &SpouseProjection::federal_withholding);
  projection.california_withholding = sum_amounts(projection.spouses, &SpouseProjection::california_withholding);
  projection.investments = project_investments(inputs, as_of, projection.warnings);
  return projection;
}

}  // namespace estimated_taxes
