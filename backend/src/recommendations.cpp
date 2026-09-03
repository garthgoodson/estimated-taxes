#include "estimated_taxes/recommendations.hpp"
#include "estimated_taxes/money.hpp"

#include <algorithm>
#include <array>
#include <chrono>

namespace estimated_taxes {
namespace {

int current_quarter(const std::string& date) {
  if (date.size() != 10 || date[4] != '-' || date[7] != '-') throw RecommendationError("as-of date must use YYYY-MM-DD");
  const int year = std::stoi(date.substr(0, 4));
  const int month = std::stoi(date.substr(5, 2));
  const int day = std::stoi(date.substr(8, 2));
  const std::chrono::year_month_day parsed{std::chrono::year{year}, std::chrono::month{static_cast<unsigned>(month)}, std::chrono::day{static_cast<unsigned>(day)}};
  if (year != 2026 || !parsed.ok()) throw RecommendationError("as-of date must be a valid 2026 date");
  return (month - 1) / 3 + 1;
}

const EstimatedPayment& payment_for(const QuarterInput& input, Jurisdiction jurisdiction) {
  return jurisdiction == Jurisdiction::federal ? input.federal_payment : input.california_payment;
}

Cents completed_payments(const TaxYearInputs& inputs, Jurisdiction jurisdiction, const std::string& as_of,
                         int through_quarter) {
  Cents total{};
  for (int index = 0; index < through_quarter; ++index) {
    const EstimatedPayment& payment = payment_for(inputs.quarters[index], jurisdiction);
    if (payment.amount_cents == 0) continue;
    if (!payment.date || *payment.date > as_of) {
      throw RecommendationError("recorded payment date is after the as-of date");
    }
    total = checked_add(total, payment.amount_cents);
  }
  return total;
}

Cents all_completed_payments(const TaxYearInputs& inputs, Jurisdiction jurisdiction, const std::string& as_of) {
  return completed_payments(inputs, jurisdiction, as_of, 4);
}

bool blocking(const std::vector<ValidationOutcome>& validation, Jurisdiction jurisdiction) {
  return std::any_of(validation.begin(), validation.end(), [jurisdiction](const ValidationOutcome& outcome) {
    return outcome.severity == ValidationSeverity::blocking && (!outcome.jurisdiction || *outcome.jurisdiction == jurisdiction);
  });
}

PaymentRecommendation recommendation(const TaxYearInputs& inputs, const std::string& as_of, int quarter,
                                     Jurisdiction jurisdiction, Cents liability, Cents withholding,
                                     const std::vector<Installment>& installments, std::int64_t revision_id,
                                     bool insufficient) {
  PaymentRecommendation result;
  result.current_quarter = quarter;
  result.rule_revision_id = revision_id;
  const Installment& current = installments.at(static_cast<size_t>(quarter - 1));
  result.due_date = current.due_date;
  result.due_date_status = as_of < current.due_date ? DueDateStatus::upcoming :
                           as_of == current.due_date ? DueDateStatus::due_today : DueDateStatus::past_due;
  if (insufficient) {
    result.calculation_status = CalculationStatus::insufficient_information;
    return result;
  }

  const Cents annual_target = liability > withholding ? checked_subtract(liability, withholding) : 0;
  const Cents prior_target = quarter == 1 ? 0 : multiply_rate(annual_target, installments.at(static_cast<size_t>(quarter - 2)).cumulative_ppm);
  const Cents cumulative_target = multiply_rate(annual_target, current.cumulative_ppm);
  const Cents credited = completed_payments(inputs, jurisdiction, as_of, quarter);
  const Cents all_paid = all_completed_payments(inputs, jurisdiction, as_of);
  const Cents recommended = cumulative_target > credited ? checked_subtract(cumulative_target, credited) : 0;
  const Cents scheduled = checked_subtract(cumulative_target, prior_target);
  const Cents catch_up = recommended > scheduled ? checked_subtract(recommended, scheduled) : 0;
  result.annual_target_cents = annual_target;
  result.cumulative_target_cents = cumulative_target;
  result.payments_credited_cents = credited;
  result.scheduled_installment_cents = scheduled;
  result.catch_up_recommendation_cents = catch_up;
  result.scheduled_recommendation_cents = checked_subtract(recommended, catch_up);
  result.recommended_payment_cents = recommended;
  result.remaining_before_recommendation_cents = annual_target > all_paid ? checked_subtract(annual_target, all_paid) : 0;
  const Cents unpaid_after = annual_target > all_paid ? checked_subtract(annual_target, all_paid) : 0;
  result.remaining_after_recommendation_cents = unpaid_after > recommended ? checked_subtract(unpaid_after, recommended) : 0;
  const Cents paid_and_withheld = checked_add(withholding, all_paid);
  result.projected_overpayment_cents = paid_and_withheld > liability ? checked_subtract(paid_and_withheld, liability) : 0;
  result.projected_overpayment = result.projected_overpayment_cents > 0;
  result.recommendation_status = catch_up > 0 ? RecommendationStatus::catch_up_recommended :
                                 recommended > 0 ? RecommendationStatus::payment_recommended :
                                                   RecommendationStatus::no_payment_currently_needed;
  for (int index = quarter; index < 4; ++index) {
    const Cents target = multiply_rate(annual_target, installments[index].cumulative_ppm);
    const Cents payments = checked_add(completed_payments(inputs, jurisdiction, as_of, index + 1), recommended);
    result.future_outlook.push_back({index + 1, target, target > payments ? checked_subtract(target, payments) : 0});
  }
  return result;
}

}  // namespace

RecommendationResult recommend_payments(const TaxYearInputs& inputs, const std::string& as_of_date,
                                        const TaxCalculation& taxes, const ActiveRules& rules,
                                        std::vector<ValidationOutcome> validation) {
  const int quarter = current_quarter(as_of_date);
  for (const QuarterInput& input : inputs.quarters) {
    for (const Jurisdiction jurisdiction : {Jurisdiction::federal, Jurisdiction::california}) {
      const EstimatedPayment& payment = payment_for(input, jurisdiction);
      if (payment.amount_cents > 0 && (!payment.date || *payment.date > as_of_date)) {
        throw RecommendationError("recorded payment date is after the as-of date");
      }
    }
  }
  if (!rules.federal.federal || !rules.california.california) throw RecommendationError("active rules are incomplete");
  validate(*rules.federal.federal);
  validate(*rules.california.california);
  RecommendationResult result;
  result.validation = std::move(validation);
  result.federal = recommendation(inputs, as_of_date, quarter, Jurisdiction::federal,
      taxes.federal.details.annual_liability_cents, taxes.federal.details.projected_withholding_cents,
      rules.federal.federal->installments, rules.federal.id, blocking(result.validation, Jurisdiction::federal));
  result.california = recommendation(inputs, as_of_date, quarter, Jurisdiction::california,
      taxes.california.details.annual_liability_cents, taxes.california.details.projected_withholding_cents,
      rules.california.california->installments, rules.california.id, blocking(result.validation, Jurisdiction::california));
  return result;
}

CurrentResult compose_current_result(const Household& household, const TaxYearInputs& inputs,
                                     const std::string& as_of_date, const ActiveRules& rules,
                                     std::vector<ValidationOutcome> validation) {
  validate(household);
  CurrentResult result;
  result.household = household;
  result.as_of_date = as_of_date;
  result.projection = project_annual(inputs, as_of_date);
  result.taxes = calculate_tax(result.projection, household, rules);
  result.recommendations = recommend_payments(inputs, as_of_date, result.taxes, rules, std::move(validation));
  return result;
}

}  // namespace estimated_taxes
