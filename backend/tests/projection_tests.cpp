#include "estimated_taxes/projection.hpp"

#include <functional>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <utility>

using namespace estimated_taxes;

namespace {

void require(bool condition, const std::string& message)
{
  if (!condition) throw std::runtime_error(message);
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

PaystubSnapshot paystub(std::string date, std::string frequency = "biweekly", Cents regular = 100,
                        Cents bonus = 0, Cents federal_current = 10, Cents california_current = 5,
                        Cents federal_ytd = 1'000, Cents california_ytd = 1'100,
                        Cents federal_withholding_ytd = 100, Cents california_withholding_ytd = 50)
{
  return PaystubSnapshot{std::move(date), std::move(frequency), regular, bonus, federal_current, california_current,
                         federal_ytd, california_ytd, federal_withholding_ytd, california_withholding_ytd, 0, 0, 0};
}

TaxYearInputs inputs()
{
  TaxYearInputs result;
  for (int quarter = 1; quarter <= 4; ++quarter) result.quarters[quarter - 1].quarter = quarter;
  return result;
}

bool has_warning(const AnnualProjection& projection, const std::string& code)
{
  for (const auto& warning : projection.warnings) {
    if (warning.code == code) return true;
  }
  return false;
}

void test_pay_frequencies_and_period_boundaries()
{
  const std::pair<const char*, int> frequencies[] = {{"weekly", 51}, {"biweekly", 25}, {"semimonthly", 23}, {"monthly", 11}};
  for (const auto& [frequency, expected] : frequencies) {
    auto year = inputs();
    year.quarters[0].spouse_1_paystub = paystub("2026-01-01", frequency);
    require(project_annual(year, "2026-01-01").spouses[0].remaining_pay_periods == expected,
            std::string(frequency) + " beginning-of-year periods");
  }
  auto end = inputs();
  end.quarters[3].spouse_1_paystub = paystub("2026-12-31", "weekly");
  require(project_annual(end, "2026-12-31").spouses[0].remaining_pay_periods == 0, "end-of-year periods");
  auto boundary = inputs();
  boundary.quarters[0].spouse_1_paystub = paystub("2026-01-07", "weekly");
  require(project_annual(boundary, "2026-01-07").spouses[0].remaining_pay_periods == 51, "rounding lower boundary");
  boundary.quarters[0].spouse_1_paystub = paystub("2026-01-08", "weekly");
  require(project_annual(boundary, "2026-01-08").spouses[0].remaining_pay_periods == 50, "rounding upper boundary");
}

void test_spouse_selection_totals_and_wage_separation()
{
  auto year = inputs();
  year.quarters[0].spouse_1_paystub = paystub("2026-03-01", "monthly", 100, 0, 10, 5, 500, 600, 50, 60);
  year.quarters[1].spouse_1_paystub = paystub("2026-06-01", "monthly", 200, 0, 20, 10, 900, 1'100, 90, 110);
  year.quarters[1].spouse_2_paystub = paystub("2026-06-01", "monthly", 50, 0, 4, 2, 300, 300, 30, 30);
  const auto projection = project_annual(year, "2026-06-15");
  require(projection.spouses[0].authoritative_quarter == 2, "latest quarter selected");
  require(projection.spouses[0].federal_wages.actual_cents == 900 && projection.spouses[0].california_wages.actual_cents == 1'100,
          "federal and California actual wages stay separate");
  require(projection.federal_wages.actual_cents == 1'200, "two-spouse total");
  require(projection.federal_wages.actual_cents + projection.federal_wages.projected_remaining_cents ==
              projection.federal_wages.projected_annual_cents &&
              projection.california_withholding.actual_cents + projection.california_withholding.projected_remaining_cents ==
                  projection.california_withholding.projected_annual_cents,
          "wage and withholding reconciliation");
  auto none = inputs();
  require(project_annual(none, "2026-06-15").federal_wages.projected_annual_cents == 0, "no paystub is zero");
}

void test_future_and_duplicate_paystubs()
{
  auto future = inputs();
  future.quarters[1].spouse_1_paystub = paystub("2026-06-30");
  require_throws<ProjectionError>([&] { static_cast<void>(project_annual(future, "2026-06-15")); }, "future paystub rejected");
  auto duplicate = inputs();
  duplicate.quarters[0].spouse_1_paystub = paystub("2026-03-01", "monthly", 100, 0, 10, 5, 500);
  duplicate.quarters[1].spouse_1_paystub = paystub("2026-03-01", "monthly", 200, 0, 20, 10, 700);
  const auto projection = project_annual(duplicate, "2026-06-15");
  require(projection.spouses[0].authoritative_quarter == 2 && has_warning(projection, "duplicate_paystub_date"),
          "later duplicate takes precedence and warns");
}

void test_regular_bonus_and_fallbacks()
{
  auto regular = inputs();
  regular.quarters[1].spouse_1_paystub = paystub("2026-06-01", "monthly", 100, 0, 10, 5, 1'000, 1'100, 100, 50);
  auto projection = project_annual(regular, "2026-06-15");
  require(projection.spouses[0].federal_wages.projected_remaining_cents == 700, "regular wages repeat");
  require(projection.spouses[0].federal_withholding.projected_remaining_cents == 70, "regular withholding repeats");

  auto earlier = inputs();
  earlier.quarters[0].spouse_1_paystub = paystub("2026-03-01", "monthly", 100, 0, 10, 4, 300, 300, 30, 12);
  earlier.quarters[1].spouse_1_paystub = paystub("2026-06-01", "monthly", 100, 50, 30, 15, 1'000, 1'100, 100, 50);
  projection = project_annual(earlier, "2026-06-15");
  require(projection.spouses[0].federal_wages.projected_remaining_cents == 700, "bonus excluded from future wages");
  require(projection.spouses[0].federal_withholding.projected_remaining_cents == 70 &&
              projection.spouses[0].california_withholding.projected_remaining_cents == 28,
          "earlier regular withholding fallback is jurisdiction-specific");

  auto proportional = inputs();
  proportional.quarters[1].spouse_1_paystub = paystub("2026-06-01", "monthly", 100, 100, 40, 30, 1'000, 1'000, 100, 100);
  projection = project_annual(proportional, "2026-06-15");
  require(projection.spouses[0].federal_withholding.projected_remaining_cents == 140 &&
              projection.spouses[0].california_withholding.projected_remaining_cents == 105 &&
              has_warning(projection, "bonus_withholding_proportional_allocation"),
          "proportional fallback");

  auto half_cent = inputs();
  half_cent.quarters[1].spouse_1_paystub = paystub("2026-06-01", "monthly", 1, 1, 1, 1, 2, 2, 1, 1);
  projection = project_annual(half_cent, "2026-06-15");
  require(projection.spouses[0].federal_withholding.projected_remaining_cents == 7,
          "proportional fallback rounds half cents away from zero");
}

void test_zero_wage_adjustments_are_not_projected()
{
  for (const Cents withholding : {Cents{0}, Cents{25}}) {
    auto year = inputs();
    year.quarters[1].spouse_1_paystub = paystub("2026-06-01", "monthly", 0, 0, withholding, withholding,
                                                 0, 0, withholding, withholding);
    const auto projection = project_annual(year, "2026-06-15");
    require(projection.spouses[0].federal_withholding.projected_remaining_cents == 0 &&
                projection.spouses[0].california_withholding.projected_remaining_cents == 0 &&
                has_warning(projection, "bonus_withholding_zero_wages"),
            "zero-wage withholding adjustments are not recurring");
  }
}

void test_investments_and_missing_warnings()
{
  auto year = inputs();
  year.quarters[0].investments = InvestmentSummary{0, 0, -10, -20, 0, 0, std::nullopt};
  year.quarters[1].investments = InvestmentSummary{40, 30, -5, 90, 7, 8, std::nullopt};
  const auto projection = project_annual(year, "2026-06-15");
  require(projection.investments.ordinary_dividends_cents == 40 && projection.investments.short_term_gain_cents == -15 &&
              projection.investments.long_term_gain_cents == 70,
          "elapsed investment totals retain negative gains");
  require(!has_warning(projection, "missing_elapsed_investment_quarter"), "explicit zero investment is not missing");
  require(has_warning(projection, "investment_income_not_projected"), "future investments are not projected");

  auto missing = inputs();
  require(has_warning(project_annual(missing, "2026-06-15"), "missing_elapsed_investment_quarter"), "missing investment warns");
  auto future = inputs();
  future.quarters[2].investments = InvestmentSummary{};
  require_throws<ProjectionError>([&] { static_cast<void>(project_annual(future, "2026-06-15")); }, "future investments rejected");
}

void test_stale_missing_and_overflow()
{
  auto stale = inputs();
  stale.quarters[0].spouse_1_paystub = paystub("2026-03-01", "weekly");
  auto projection = project_annual(stale, "2026-06-15");
  require(has_warning(projection, "stale_paystub") && has_warning(projection, "missing_current_quarter_paystub"),
          "stale and missing-current warnings");
  auto boundary = inputs();
  boundary.quarters[1].spouse_1_paystub = paystub("2026-06-01", "weekly");
  require(!has_warning(project_annual(boundary, "2026-06-15"), "stale_paystub"), "14-day stale boundary is not stale");
  require(has_warning(project_annual(boundary, "2026-06-16"), "stale_paystub"), "15-day stale boundary warns");

  auto overflow = inputs();
  overflow.quarters[0].spouse_1_paystub = paystub("2026-01-01", "weekly", std::numeric_limits<Cents>::max(), 0,
                                                   0, 0, std::numeric_limits<Cents>::max(), std::numeric_limits<Cents>::max());
  require_throws<ProjectionError>([&] { static_cast<void>(project_annual(overflow, "2026-01-01")); }, "overflow is rejected");
}

}  // namespace

int main()
{
  const std::pair<const char*, std::function<void()>> tests[] = {
      {"pay frequencies and period boundaries", test_pay_frequencies_and_period_boundaries},
      {"spouse selection, totals, and wage separation", test_spouse_selection_totals_and_wage_separation},
      {"future and duplicate paystubs", test_future_and_duplicate_paystubs},
      {"regular, bonus, and withholding fallbacks", test_regular_bonus_and_fallbacks},
      {"zero-wage withholding adjustments", test_zero_wage_adjustments_are_not_projected},
      {"investments and missing warnings", test_investments_and_missing_warnings},
      {"stale, missing, and overflow", test_stale_missing_and_overflow},
  };
  int failures = 0;
  for (const auto& [name, test] : tests) {
    try { test(); std::cout << "PASS: " << name << '\n'; }
    catch (const std::exception& error) { ++failures; std::cerr << "FAIL: " << name << ": " << error.what() << '\n'; }
  }
  return failures == 0 ? 0 : 1;
}
