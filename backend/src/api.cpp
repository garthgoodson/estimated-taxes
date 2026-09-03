#include "estimated_taxes/api.hpp"

#include "estimated_taxes/database_operations.hpp"
#include "estimated_taxes/http_server.hpp"
#include "estimated_taxes/input_store.hpp"
#include "estimated_taxes/recommendations.hpp"
#include "estimated_taxes/snapshot_store.hpp"
#include "estimated_taxes/tax_rules.hpp"

#include <jansson.h>

#include <algorithm>
#include <charconv>
#include <cctype>
#include <cstdlib>
#include <initializer_list>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string_view>
#include <tuple>
#include <utility>

namespace estimated_taxes {
namespace {

class Json {
public:
  explicit Json(json_t* value) : value_(value) {}
  ~Json() { json_decref(value_); }
  Json(const Json&) = delete;
  Json& operator=(const Json&) = delete;
  [[nodiscard]] json_t* get() const { return value_; }
private:
  json_t* value_;
};

class RequestShapeError : public std::runtime_error {
public:
  using std::runtime_error::runtime_error;
};

ApiResponse json_response(int status, json_t* value)
{
  Json json(value);
  char* encoded = json_dumps(json.get(), JSON_COMPACT);
  if (encoded == nullptr) {
    return {500, {{"Content-Type", "application/json"}},
            "{\"error\":{\"code\":\"internal_error\",\"message\":\"The request could not be completed.\"}}"};
  }
  std::string body(encoded);
  std::free(encoded);
  return {status, {{"Content-Type", "application/json"}}, std::move(body)};
}

ApiResponse error_response(int status, const char* code, const char* message)
{
  json_t* error = json_object();
  json_object_set_new(error, "code", json_string(code));
  json_object_set_new(error, "message", json_string(message));
  json_t* result = json_object();
  json_object_set_new(result, "error", error);
  return json_response(status, result);
}

ApiResponse validation_error_response(std::string_view detail, std::string_view path = "request",
                                      std::string_view code = "invalid_value")
{
  json_t* field = json_object();
  json_object_set_new(field, "path", json_stringn(path.data(), path.size()));
  json_object_set_new(field, "code", json_stringn(code.data(), code.size()));
  json_object_set_new(field, "message", json_stringn(detail.data(), detail.size()));
  json_t* fields = json_array();
  json_array_append_new(fields, field);
  json_t* error = json_object();
  json_object_set_new(error, "code", json_string("validation_failed"));
  json_object_set_new(error, "message", json_string("The request contains invalid values."));
  json_object_set_new(error, "fields", fields);
  json_t* result = json_object();
  json_object_set_new(result, "error", error);
  return json_response(422, result);
}

bool is_allowed(std::string_view key, std::initializer_list<std::string_view> allowed)
{
  return std::find(allowed.begin(), allowed.end(), key) != allowed.end();
}

void require_object(json_t* value, std::initializer_list<std::string_view> fields, std::string_view path)
{
  if (!json_is_object(value)) throw RequestShapeError(std::string(path) + " must be an object");
  const char* key{};
  json_t* child{};
  json_object_foreach(value, key, child) {
    if (!is_allowed(key, fields)) throw RequestShapeError(std::string(path) + " contains unknown field " + key);
  }
}

json_t* required(json_t* object, const char* name, std::string_view path)
{
  json_t* value = json_object_get(object, name);
  if (value == nullptr) throw RequestShapeError(std::string(path) + "." + name + " is required");
  return value;
}

std::string string_value(json_t* object, const char* name, std::string_view path)
{
  json_t* value = required(object, name, path);
  if (!json_is_string(value)) throw RequestShapeError(std::string(path) + "." + name + " must be a string");
  return json_string_value(value);
}

Cents integer_value(json_t* object, const char* name, std::string_view path)
{
  json_t* value = required(object, name, path);
  if (!json_is_integer(value)) throw RequestShapeError(std::string(path) + "." + name + " must be an integer");
  return json_integer_value(value);
}

int int_value(json_t* object, const char* name, std::string_view path)
{
  const Cents value = integer_value(object, name, path);
  if (value < std::numeric_limits<int>::min() || value > std::numeric_limits<int>::max()) {
    throw ValidationError(std::string(path) + "." + name + " is outside the supported range");
  }
  return static_cast<int>(value);
}

bool boolean_value(json_t* object, const char* name, std::string_view path)
{
  json_t* value = required(object, name, path);
  if (!json_is_boolean(value)) throw RequestShapeError(std::string(path) + "." + name + " must be a boolean");
  return json_is_true(value);
}

std::optional<std::string> optional_string(json_t* object, const char* name, std::string_view path)
{
  json_t* value = required(object, name, path);
  if (json_is_null(value)) return std::nullopt;
  if (!json_is_string(value)) throw RequestShapeError(std::string(path) + "." + name + " must be a string or null");
  return std::string(json_string_value(value));
}

Json parse_json(const std::string& body)
{
  json_error_t error{};
  json_t* value = json_loadb(body.data(), body.size(), JSON_REJECT_DUPLICATES, &error);
  if (value == nullptr) throw RequestShapeError("request body is malformed JSON");
  return Json(value);
}

const char* spouse_key_name(SpouseKey key) { return key == SpouseKey::spouse_1 ? "spouse_1" : "spouse_2"; }
const char* jurisdiction_name(Jurisdiction value) { return value == Jurisdiction::federal ? "federal" : "california"; }

json_t* optional_integer_json(const std::optional<Cents>& value)
{
  return value ? json_integer(*value) : json_null();
}

json_t* optional_string_json(const std::optional<std::string>& value)
{
  return value ? json_string(value->c_str()) : json_null();
}

json_t* warning_json(const ProjectionWarning& warning)
{
  json_t* value = json_object();
  json_object_set_new(value, "severity", json_string(warning.severity == WarningSeverity::caution ? "caution" : "information"));
  json_object_set_new(value, "code", json_string(warning.code.c_str()));
  json_object_set_new(value, "path", json_string(warning.path.c_str()));
  json_object_set_new(value, "message", json_string(warning.message.c_str()));
  return value;
}

json_t* warnings_json(const std::vector<ProjectionWarning>& warnings)
{
  json_t* values = json_array();
  for (const auto& warning : warnings) json_array_append_new(values, warning_json(warning));
  return values;
}

json_t* projection_amounts_json(const ProjectionAmounts& amounts)
{
  json_t* value = json_object();
  json_object_set_new(value, "actual_cents", json_integer(amounts.actual_cents));
  json_object_set_new(value, "projected_remaining_cents", json_integer(amounts.projected_remaining_cents));
  json_object_set_new(value, "projected_annual_cents", json_integer(amounts.projected_annual_cents));
  return value;
}

json_t* projection_json(const AnnualProjection& projection)
{
  json_t* value = json_object();
  json_object_set_new(value, "federal_wages", projection_amounts_json(projection.federal_wages));
  json_object_set_new(value, "california_wages", projection_amounts_json(projection.california_wages));
  json_object_set_new(value, "federal_withholding", projection_amounts_json(projection.federal_withholding));
  json_object_set_new(value, "california_withholding", projection_amounts_json(projection.california_withholding));
  json_t* investments = json_object();
  json_object_set_new(investments, "ordinary_dividends_cents", json_integer(projection.investments.ordinary_dividends_cents));
  json_object_set_new(investments, "qualified_dividends_cents", json_integer(projection.investments.qualified_dividends_cents));
  json_object_set_new(investments, "short_term_gain_cents", json_integer(projection.investments.short_term_gain_cents));
  json_object_set_new(investments, "long_term_gain_cents", json_integer(projection.investments.long_term_gain_cents));
  json_object_set_new(investments, "federal_withholding_cents", json_integer(projection.investments.federal_withholding_cents));
  json_object_set_new(investments, "california_withholding_cents", json_integer(projection.investments.california_withholding_cents));
  json_object_set_new(value, "investments", investments);
  json_t* spouses = json_array();
  for (const auto& spouse : projection.spouses) {
    json_t* item = json_object();
    json_object_set_new(item, "key", json_string(spouse_key_name(spouse.spouse)));
    json_object_set_new(item, "authoritative_quarter", spouse.authoritative_quarter ? json_integer(*spouse.authoritative_quarter) : json_null());
    json_object_set_new(item, "remaining_pay_periods", json_integer(spouse.remaining_pay_periods));
    json_object_set_new(item, "federal_wages", projection_amounts_json(spouse.federal_wages));
    json_object_set_new(item, "california_wages", projection_amounts_json(spouse.california_wages));
    json_object_set_new(item, "federal_withholding", projection_amounts_json(spouse.federal_withholding));
    json_object_set_new(item, "california_withholding", projection_amounts_json(spouse.california_withholding));
    json_array_append_new(spouses, item);
  }
  json_object_set_new(value, "spouses", spouses);
  json_object_set_new(value, "warnings", warnings_json(projection.warnings));
  return value;
}

json_t* tax_details_json(const TaxCalculationDetails& details)
{
  json_t* value = json_object();
  const std::pair<const char*, Cents> fields[] = {
      {"supported_agi_cents", details.supported_agi_cents}, {"deduction_cents", details.deduction_cents},
      {"taxable_income_cents", details.taxable_income_cents}, {"preferential_pool_cents", details.preferential_pool_cents},
      {"ordinary_taxable_income_cents", details.ordinary_taxable_income_cents}, {"ordinary_tax_cents", details.ordinary_tax_cents},
      {"preferential_tax_cents", details.preferential_tax_cents}, {"ordinary_tax_on_all_income_cents", details.ordinary_tax_on_all_income_cents},
      {"regular_tax_cents", details.regular_tax_cents}, {"exemption_credit_cents", details.exemption_credit_cents},
      {"tax_before_exemption_cents", details.tax_before_exemption_cents}, {"tax_after_exemption_cents", details.tax_after_exemption_cents},
      {"niit_cents", details.niit_cents}, {"additional_medicare_cents", details.additional_medicare_cents},
      {"behavioral_health_services_cents", details.behavioral_health_services_cents}, {"annual_liability_cents", details.annual_liability_cents},
      {"projected_withholding_cents", details.projected_withholding_cents},
      {"amount_requiring_estimated_payments_cents", details.amount_requiring_estimated_payments_cents}};
  for (const auto& [name, amount] : fields) json_object_set_new(value, name, json_integer(amount));
  return value;
}

json_t* tax_result_json(const TaxResult& result)
{
  json_t* value = json_object();
  json_object_set_new(value, "details", tax_details_json(result.details));
  json_t* netting = json_object();
  json_object_set_new(netting, "ordinary_gain_cents", json_integer(result.capital_netting.ordinary_gain_cents));
  json_object_set_new(netting, "preferential_gain_cents", json_integer(result.capital_netting.preferential_gain_cents));
  json_object_set_new(netting, "deductible_loss_cents", json_integer(result.capital_netting.deductible_loss_cents));
  json_object_set_new(netting, "unused_loss_cents", json_integer(result.capital_netting.unused_loss_cents));
  json_object_set_new(value, "capital_netting", netting);
  json_object_set_new(value, "effective_rate_ppm", json_integer(result.effective_rate_ppm));
  json_object_set_new(value, "rule_revision_id", json_string(std::to_string(result.rule_revision_id).c_str()));
  json_object_set_new(value, "warnings", warnings_json(result.warnings));
  return value;
}

const char* calculation_status_name(CalculationStatus status)
{
  return status == CalculationStatus::available ? "available" : "insufficient_information";
}

const char* recommendation_status_name(RecommendationStatus status)
{
  switch (status) {
    case RecommendationStatus::payment_recommended: return "payment_recommended";
    case RecommendationStatus::catch_up_recommended: return "catch_up_recommended";
    case RecommendationStatus::no_payment_currently_needed: return "no_payment_currently_needed";
  }
  return "no_payment_currently_needed";
}

const char* due_date_status_name(DueDateStatus status)
{
  switch (status) {
    case DueDateStatus::upcoming: return "upcoming";
    case DueDateStatus::due_today: return "due_today";
    case DueDateStatus::past_due: return "past_due";
  }
  return "upcoming";
}

json_t* recommendation_json(const PaymentRecommendation& recommendation)
{
  json_t* value = json_object();
  json_object_set_new(value, "calculation_status", json_string(calculation_status_name(recommendation.calculation_status)));
  json_object_set_new(value, "recommendation_status", recommendation.recommendation_status ? json_string(recommendation_status_name(*recommendation.recommendation_status)) : json_null());
  json_object_set_new(value, "due_date_status", recommendation.due_date_status ? json_string(due_date_status_name(*recommendation.due_date_status)) : json_null());
  json_object_set_new(value, "projected_overpayment_cents", json_integer(recommendation.projected_overpayment_cents));
  json_object_set_new(value, "projected_overpayment", json_boolean(recommendation.projected_overpayment));
  json_object_set_new(value, "annual_target_cents", optional_integer_json(recommendation.annual_target_cents));
  json_object_set_new(value, "cumulative_target_cents", optional_integer_json(recommendation.cumulative_target_cents));
  json_object_set_new(value, "payments_credited_cents", optional_integer_json(recommendation.payments_credited_cents));
  json_object_set_new(value, "scheduled_installment_cents", optional_integer_json(recommendation.scheduled_installment_cents));
  json_object_set_new(value, "scheduled_recommendation_cents", optional_integer_json(recommendation.scheduled_recommendation_cents));
  json_object_set_new(value, "catch_up_recommendation_cents", optional_integer_json(recommendation.catch_up_recommendation_cents));
  json_object_set_new(value, "recommended_payment_cents", optional_integer_json(recommendation.recommended_payment_cents));
  json_object_set_new(value, "remaining_before_recommendation_cents", optional_integer_json(recommendation.remaining_before_recommendation_cents));
  json_object_set_new(value, "remaining_after_recommendation_cents", optional_integer_json(recommendation.remaining_after_recommendation_cents));
  json_object_set_new(value, "current_quarter", json_integer(recommendation.current_quarter));
  json_object_set_new(value, "due_date", optional_string_json(recommendation.due_date));
  json_t* outlook = json_array();
  for (const auto& item : recommendation.future_outlook) {
    json_t* entry = json_object();
    json_object_set_new(entry, "quarter", json_integer(item.quarter));
    json_object_set_new(entry, "cumulative_target_cents", json_integer(item.cumulative_target_cents));
    json_object_set_new(entry, "recommended_payment_cents", json_integer(item.recommended_payment_cents));
    json_array_append_new(outlook, entry);
  }
  json_object_set_new(value, "future_outlook", outlook);
  json_object_set_new(value, "rule_revision_id", json_string(std::to_string(recommendation.rule_revision_id).c_str()));
  return value;
}

json_t* validation_json(const ValidationOutcome& outcome)
{
  json_t* value = json_object();
  const char* severity = outcome.severity == ValidationSeverity::blocking ? "blocking" :
                         outcome.severity == ValidationSeverity::caution ? "caution" : "informational";
  json_object_set_new(value, "severity", json_string(severity));
  json_object_set_new(value, "jurisdiction", outcome.jurisdiction ? json_string(jurisdiction_name(*outcome.jurisdiction)) : json_null());
  json_object_set_new(value, "code", json_string(outcome.code.c_str()));
  json_object_set_new(value, "message", json_string(outcome.message.c_str()));
  return value;
}

json_t* recommendations_json(const RecommendationResult& result)
{
  json_t* value = json_object();
  json_object_set_new(value, "federal", recommendation_json(result.federal));
  json_object_set_new(value, "california", recommendation_json(result.california));
  json_t* validation = json_array();
  for (const auto& outcome : result.validation) json_array_append_new(validation, validation_json(outcome));
  json_object_set_new(value, "validation", validation);
  return value;
}

json_t* current_result_json(const CurrentResult& result)
{
  json_t* value = json_object();
  json_object_set_new(value, "as_of_date", json_string(result.as_of_date.c_str()));
  json_object_set_new(value, "projection", projection_json(result.projection));
  json_t* tax = json_object();
  json_object_set_new(tax, "federal", tax_result_json(result.taxes.federal));
  json_object_set_new(tax, "california", tax_result_json(result.taxes.california));
  json_object_set_new(value, "tax", tax);
  json_object_set_new(value, "recommendations", recommendations_json(result.recommendations));
  return value;
}

json_t* spouse_json(const Spouse& spouse)
{
  json_t* value = json_object();
  json_object_set_new(value, "key", json_string(spouse_key_name(spouse.key)));
  json_object_set_new(value, "label", json_string(spouse.label.c_str()));
  json_object_set_new(value, "age_65_or_older", json_boolean(spouse.age_65_or_older));
  json_object_set_new(value, "blind", json_boolean(spouse.blind));
  return value;
}

json_t* household_json(const Household& household)
{
  json_t* value = json_object();
  json_object_set_new(value, "tax_year", json_integer(2026));
  json_object_set_new(value, "filing_status", json_string("married_filing_jointly"));
  json_object_set_new(value, "residency", json_string("california_full_year"));
  json_t* spouses = json_array();
  json_array_append_new(spouses, spouse_json(household.spouse_1));
  json_array_append_new(spouses, spouse_json(household.spouse_2));
  json_object_set_new(value, "spouses", spouses);
  return value;
}

json_t* paystub_json(const std::optional<PaystubSnapshot>& paystub)
{
  if (!paystub) return json_null();
  json_t* value = json_object();
  json_object_set_new(value, "date", json_string(paystub->date.c_str()));
  json_object_set_new(value, "pay_frequency", json_string(paystub->pay_frequency.c_str()));
  const std::pair<const char*, Cents> fields[] = {
      {"current_period_regular_wages_cents", paystub->current_period_regular_wages_cents},
      {"current_period_bonus_wages_cents", paystub->current_period_bonus_wages_cents},
      {"current_period_federal_withholding_cents", paystub->current_period_federal_withholding_cents},
      {"current_period_california_withholding_cents", paystub->current_period_california_withholding_cents},
      {"federal_taxable_wages_ytd_cents", paystub->federal_taxable_wages_ytd_cents},
      {"california_taxable_wages_ytd_cents", paystub->california_taxable_wages_ytd_cents},
      {"federal_withholding_ytd_cents", paystub->federal_withholding_ytd_cents},
      {"california_withholding_ytd_cents", paystub->california_withholding_ytd_cents},
      {"social_security_withholding_ytd_cents", paystub->social_security_withholding_ytd_cents},
      {"medicare_withholding_ytd_cents", paystub->medicare_withholding_ytd_cents},
      {"california_sdi_withholding_ytd_cents", paystub->california_sdi_withholding_ytd_cents}};
  for (const auto& [name, amount] : fields) json_object_set_new(value, name, json_integer(amount));
  return value;
}

json_t* quarter_input_json(const QuarterInput& input)
{
  json_t* value = json_object();
  json_t* paystubs = json_object();
  json_object_set_new(paystubs, "spouse_1", paystub_json(input.spouse_1_paystub));
  json_object_set_new(paystubs, "spouse_2", paystub_json(input.spouse_2_paystub));
  json_object_set_new(value, "paystubs", paystubs);
  if (input.investments) {
    json_t* investments = json_object();
    json_object_set_new(investments, "ordinary_dividends_cents", json_integer(input.investments->ordinary_dividends_cents));
    json_object_set_new(investments, "qualified_dividends_cents", json_integer(input.investments->qualified_dividends_cents));
    json_object_set_new(investments, "short_term_gain_cents", json_integer(input.investments->short_term_gain_cents));
    json_object_set_new(investments, "long_term_gain_cents", json_integer(input.investments->long_term_gain_cents));
    json_object_set_new(investments, "federal_withholding_cents", json_integer(input.investments->federal_withholding_cents));
    json_object_set_new(investments, "california_withholding_cents", json_integer(input.investments->california_withholding_cents));
    json_object_set_new(investments, "notes", optional_string_json(input.investments->notes));
    json_object_set_new(value, "investments", investments);
  } else {
    json_object_set_new(value, "investments", json_null());
  }
  json_t* payments = json_object();
  for (const auto [name, payment] : {std::pair{"federal", &input.federal_payment}, std::pair{"california", &input.california_payment}}) {
    json_t* item = json_object();
    json_object_set_new(item, "amount_cents", json_integer(payment->amount_cents));
    json_object_set_new(item, "date", optional_string_json(payment->date));
    json_object_set_new(payments, name, item);
  }
  json_object_set_new(value, "payments", payments);
  return value;
}

json_t* bracket_json(const Bracket& bracket)
{
  json_t* value = json_object();
  json_object_set_new(value, "lower_bound_cents", json_integer(bracket.lower_bound_cents));
  json_object_set_new(value, "upper_bound_cents", bracket.upper_bound_cents ? json_integer(*bracket.upper_bound_cents) : json_null());
  json_object_set_new(value, "rate_ppm", json_integer(bracket.rate_ppm));
  return value;
}

json_t* installments_json(const std::vector<Installment>& installments)
{
  json_t* values = json_array();
  for (const auto& installment : installments) {
    json_t* value = json_object();
    json_object_set_new(value, "quarter", json_integer(installment.quarter));
    json_object_set_new(value, "due_date", json_string(installment.due_date.c_str()));
    json_object_set_new(value, "period_ppm", json_integer(installment.period_ppm));
    json_object_set_new(value, "cumulative_ppm", json_integer(installment.cumulative_ppm));
    json_array_append_new(values, value);
  }
  return values;
}

json_t* brackets_json(const std::vector<Bracket>& brackets)
{
  json_t* values = json_array();
  for (const auto& bracket : brackets) json_array_append_new(values, bracket_json(bracket));
  return values;
}

json_t* sources_json(const std::vector<Source>& sources)
{
  json_t* values = json_array();
  for (const auto& source : sources) {
    json_t* value = json_object();
    json_object_set_new(value, "agency", json_string(source.agency.c_str()));
    json_object_set_new(value, "title", json_string(source.title.c_str()));
    json_object_set_new(value, "url", json_string(source.url.c_str()));
    json_object_set_new(value, "publication_date", json_string(source.publication_date.c_str()));
    json_object_set_new(value, "verification_date", json_string(source.verification_date.c_str()));
    json_object_set_new(value, "interpretation_note", json_string(source.interpretation_note.c_str()));
    json_array_append_new(values, value);
  }
  return values;
}

json_t* federal_rules_json(const FederalRules& rules)
{
  json_t* value = json_object();
  json_object_set_new(value, "tax_year", json_integer(rules.tax_year));
  json_object_set_new(value, "standard_deduction_cents", json_integer(rules.standard_deduction_cents));
  json_object_set_new(value, "age_or_blind_addition_cents", json_integer(rules.age_or_blind_addition_cents));
  json_object_set_new(value, "ordinary_brackets", brackets_json(rules.ordinary_brackets));
  json_object_set_new(value, "preferential_brackets", brackets_json(rules.preferential_brackets));
  json_object_set_new(value, "capital_loss_limit_cents", json_integer(rules.capital_loss_limit_cents));
  json_t* additional = json_object();
  json_t* niit = json_object();
  json_object_set_new(niit, "threshold_cents", json_integer(rules.niit_threshold_cents));
  json_object_set_new(niit, "rate_ppm", json_integer(rules.niit_rate_ppm));
  json_object_set_new(additional, "niit", niit);
  json_t* medicare = json_object();
  json_object_set_new(medicare, "threshold_cents", json_integer(rules.additional_medicare_threshold_cents));
  json_object_set_new(medicare, "rate_ppm", json_integer(rules.additional_medicare_rate_ppm));
  json_object_set_new(additional, "additional_medicare", medicare);
  json_object_set_new(value, "additional_taxes", additional);
  json_object_set_new(value, "installments", installments_json(rules.installments));
  return value;
}

json_t* california_rules_json(const CaliforniaRules& rules)
{
  json_t* value = json_object();
  json_object_set_new(value, "tax_year", json_integer(rules.tax_year));
  json_object_set_new(value, "standard_deduction_cents", json_integer(rules.standard_deduction_cents));
  json_object_set_new(value, "ordinary_brackets", brackets_json(rules.ordinary_brackets));
  json_object_set_new(value, "joint_personal_exemption_credit_cents", json_integer(rules.joint_personal_exemption_credit_cents));
  json_object_set_new(value, "capital_loss_limit_cents", json_integer(rules.capital_loss_limit_cents));
  json_t* additional = json_object();
  json_t* behavioral = json_object();
  json_object_set_new(behavioral, "threshold_cents", json_integer(rules.behavioral_health_services_threshold_cents));
  json_object_set_new(behavioral, "rate_ppm", json_integer(rules.behavioral_health_services_rate_ppm));
  json_object_set_new(additional, "behavioral_health_services", behavioral);
  json_object_set_new(value, "additional_taxes", additional);
  json_object_set_new(value, "installments", installments_json(rules.installments));
  return value;
}

json_t* rule_revision_json(const RuleRevision& revision)
{
  json_t* value = json_object();
  json_object_set_new(value, "revision_id", json_string(std::to_string(revision.id).c_str()));
  json_object_set_new(value, "jurisdiction", json_string(jurisdiction_name(revision.jurisdiction)));
  json_object_set_new(value, "official_baseline", json_boolean(revision.official_baseline));
  json_object_set_new(value, "modified", json_boolean(revision.modified));
  json_object_set_new(value, "sources", sources_json(revision.sources));
  json_object_set_new(value, "rules", revision.federal ? federal_rules_json(*revision.federal) : california_rules_json(*revision.california));
  return value;
}

json_t* active_rules_json(const ActiveRules& active)
{
  json_t* value = json_object();
  json_object_set_new(value, "federal", rule_revision_json(active.federal));
  json_object_set_new(value, "california", rule_revision_json(active.california));
  return value;
}

json_t* rule_resource_json(const ActiveRules& active, const RuleStore& store)
{
  json_t* value = json_object();
  for (const auto [name, revision, customized] : {
           std::tuple{"federal", &active.federal, active.federal_customized},
           std::tuple{"california", &active.california, active.california_customized}}) {
    json_t* item = revision->federal ? federal_rules_json(*revision->federal) : california_rules_json(*revision->california);
    json_object_set_new(item, "revision_id", json_string(std::to_string(revision->id).c_str()));
    json_object_set_new(item, "official", json_boolean(!customized));
    json_object_set_new(item, "sources", sources_json(revision->sources));
    json_t* archived = json_array();
    for (const auto& prior : store.archived_revisions(revision->jurisdiction)) {
      json_array_append_new(archived, rule_revision_json(prior));
    }
    json_object_set_new(item, "archived_revisions", archived);
    json_object_set_new(value, name, item);
  }
  return value;
}

TaxYearInputs load_inputs(const InputStore& store)
{
  TaxYearInputs inputs;
  for (int quarter = 1; quarter <= 4; ++quarter) inputs.quarters[quarter - 1] = store.load_quarter(quarter);
  return inputs;
}

struct CurrentState {
  Household household;
  TaxYearInputs inputs;
  ActiveRules rules;
};

CurrentState load_current_state(const InputStore& inputs, const RuleStore& rules)
{
  return {inputs.load_household(), load_inputs(inputs), rules.load_active()};
}

CurrentResult compose(const CurrentState& state, const std::string& as_of_date)
{
  return compose_current_result(state.household, state.inputs, as_of_date, state.rules);
}

int quarter_from_date(const std::string& date)
{
  if (date.size() < 7) throw ValidationError("as-of date is invalid");
  const int month = std::stoi(date.substr(5, 2));
  return (month - 1) / 3 + 1;
}

bool quarter_has_facts(const QuarterInput& quarter)
{
  return quarter.spouse_1_paystub || quarter.spouse_2_paystub || quarter.investments ||
         quarter.federal_payment.amount_cents != 0 || quarter.california_payment.amount_cents != 0;
}

json_t* bootstrap_json(const CurrentResult& result, const TaxYearInputs& inputs)
{
  json_t* response = json_object();
  json_object_set_new(response, "tax_year", json_integer(2026));
  json_object_set_new(response, "as_of_date", json_string(result.as_of_date.c_str()));
  json_object_set_new(response, "current_quarter", json_integer(result.recommendations.federal.current_quarter));
  json_t* household = json_object();
  json_t* spouses = json_array();
  for (const Spouse* spouse : {&result.household.spouse_1, &result.household.spouse_2}) {
    json_t* item = json_object();
    json_object_set_new(item, "key", json_string(spouse_key_name(spouse->key)));
    json_object_set_new(item, "label", json_string(spouse->label.c_str()));
    json_array_append_new(spouses, item);
  }
  json_object_set_new(household, "spouses", spouses);
  json_object_set_new(response, "household", household);

  Cents federal_payments{}, california_payments{};
  for (const auto& quarter : inputs.quarters) {
    federal_payments += quarter.federal_payment.amount_cents;
    california_payments += quarter.california_payment.amount_cents;
  }
  json_t* actuals = json_object();
  json_object_set_new(actuals, "federal_wages_ytd_cents", json_integer(result.projection.federal_wages.actual_cents));
  json_object_set_new(actuals, "california_wages_ytd_cents", json_integer(result.projection.california_wages.actual_cents));
  json_object_set_new(actuals, "ordinary_dividends_cents", json_integer(result.projection.investments.ordinary_dividends_cents));
  json_object_set_new(actuals, "qualified_dividends_cents", json_integer(result.projection.investments.qualified_dividends_cents));
  json_object_set_new(actuals, "short_term_gain_cents", json_integer(result.projection.investments.short_term_gain_cents));
  json_object_set_new(actuals, "long_term_gain_cents", json_integer(result.projection.investments.long_term_gain_cents));
  json_object_set_new(actuals, "federal_withholding_ytd_cents", json_integer(result.projection.federal_withholding.actual_cents));
  json_object_set_new(actuals, "california_withholding_ytd_cents", json_integer(result.projection.california_withholding.actual_cents));
  json_object_set_new(actuals, "federal_estimated_payments_cents", json_integer(federal_payments));
  json_object_set_new(actuals, "california_estimated_payments_cents", json_integer(california_payments));
  json_object_set_new(response, "actuals", actuals);
  json_t* projection = json_object();
  json_object_set_new(projection, "federal_wages_cents", json_integer(result.projection.federal_wages.projected_annual_cents));
  json_object_set_new(projection, "california_wages_cents", json_integer(result.projection.california_wages.projected_annual_cents));
  json_object_set_new(projection, "federal_withholding_cents", json_integer(result.projection.federal_withholding.projected_annual_cents));
  json_object_set_new(projection, "california_withholding_cents", json_integer(result.projection.california_withholding.projected_annual_cents));
  json_object_set_new(response, "projection", projection);
  json_t* tax = json_object();
  for (const auto [name, pair] : {std::pair{"federal", std::pair{&result.taxes.federal, &result.recommendations.federal}},
                                  std::pair{"california", std::pair{&result.taxes.california, &result.recommendations.california}}}) {
    json_t* item = json_object();
    json_object_set_new(item, "annual_liability_cents", json_integer(pair.first->details.annual_liability_cents));
    json_object_set_new(item, "remaining_obligation_cents", optional_integer_json(pair.second->remaining_before_recommendation_cents));
    json_object_set_new(item, "current_recommendation_cents", optional_integer_json(pair.second->recommended_payment_cents));
    json_object_set_new(tax, name, item);
  }
  json_object_set_new(response, "tax", tax);
  json_t* quarters = json_array();
  const int current_quarter = quarter_from_date(result.as_of_date);
  for (const auto& quarter : inputs.quarters) {
    json_t* item = json_object();
    json_object_set_new(item, "quarter", json_integer(quarter.quarter));
    const char* status = !quarter_has_facts(quarter) ? "not_started" : quarter.quarter < current_quarter ? "complete" : "in_progress";
    json_object_set_new(item, "status", json_string(status));
    json_array_append_new(quarters, item);
  }
  json_object_set_new(response, "quarters", quarters);
  json_object_set_new(response, "warnings", warnings_json(result.projection.warnings));
  return response;
}

PaystubSnapshot parse_paystub(json_t* value, std::string_view path)
{
  require_object(value, {"date", "pay_frequency", "current_period_regular_wages_cents", "current_period_bonus_wages_cents",
                         "current_period_federal_withholding_cents", "current_period_california_withholding_cents",
                         "federal_taxable_wages_ytd_cents", "california_taxable_wages_ytd_cents",
                         "federal_withholding_ytd_cents", "california_withholding_ytd_cents",
                         "social_security_withholding_ytd_cents", "medicare_withholding_ytd_cents",
                         "california_sdi_withholding_ytd_cents"}, path);
  PaystubSnapshot paystub;
  paystub.date = string_value(value, "date", path);
  paystub.pay_frequency = string_value(value, "pay_frequency", path);
  paystub.current_period_regular_wages_cents = integer_value(value, "current_period_regular_wages_cents", path);
  paystub.current_period_bonus_wages_cents = integer_value(value, "current_period_bonus_wages_cents", path);
  paystub.current_period_federal_withholding_cents = integer_value(value, "current_period_federal_withholding_cents", path);
  paystub.current_period_california_withholding_cents = integer_value(value, "current_period_california_withholding_cents", path);
  paystub.federal_taxable_wages_ytd_cents = integer_value(value, "federal_taxable_wages_ytd_cents", path);
  paystub.california_taxable_wages_ytd_cents = integer_value(value, "california_taxable_wages_ytd_cents", path);
  paystub.federal_withholding_ytd_cents = integer_value(value, "federal_withholding_ytd_cents", path);
  paystub.california_withholding_ytd_cents = integer_value(value, "california_withholding_ytd_cents", path);
  paystub.social_security_withholding_ytd_cents = integer_value(value, "social_security_withholding_ytd_cents", path);
  paystub.medicare_withholding_ytd_cents = integer_value(value, "medicare_withholding_ytd_cents", path);
  paystub.california_sdi_withholding_ytd_cents = integer_value(value, "california_sdi_withholding_ytd_cents", path);
  return paystub;
}

std::optional<PaystubSnapshot> parse_optional_paystub(json_t* value, std::string_view path)
{
  if (json_is_null(value)) return std::nullopt;
  return parse_paystub(value, path);
}

EstimatedPayment parse_payment(json_t* value, std::string_view path)
{
  require_object(value, {"amount_cents", "date"}, path);
  return {integer_value(value, "amount_cents", path), optional_string(value, "date", path)};
}

QuarterInput parse_quarter(json_t* root, int quarter)
{
  require_object(root, {"paystubs", "investments", "payments"}, "quarter");
  QuarterInput result;
  result.quarter = quarter;
  json_t* paystubs = required(root, "paystubs", "quarter");
  require_object(paystubs, {"spouse_1", "spouse_2"}, "paystubs");
  result.spouse_1_paystub = parse_optional_paystub(required(paystubs, "spouse_1", "paystubs"), "paystubs.spouse_1");
  result.spouse_2_paystub = parse_optional_paystub(required(paystubs, "spouse_2", "paystubs"), "paystubs.spouse_2");
  json_t* investments = required(root, "investments", "quarter");
  if (!json_is_null(investments)) {
    require_object(investments, {"ordinary_dividends_cents", "qualified_dividends_cents", "short_term_gain_cents",
                                 "long_term_gain_cents", "federal_withholding_cents", "california_withholding_cents", "notes"}, "investments");
    result.investments = InvestmentSummary{integer_value(investments, "ordinary_dividends_cents", "investments"),
                                           integer_value(investments, "qualified_dividends_cents", "investments"),
                                           integer_value(investments, "short_term_gain_cents", "investments"),
                                           integer_value(investments, "long_term_gain_cents", "investments"),
                                           integer_value(investments, "federal_withholding_cents", "investments"),
                                           integer_value(investments, "california_withholding_cents", "investments"),
                                           optional_string(investments, "notes", "investments")};
  }
  json_t* payments = required(root, "payments", "quarter");
  require_object(payments, {"federal", "california"}, "payments");
  result.federal_payment = parse_payment(required(payments, "federal", "payments"), "payments.federal");
  result.california_payment = parse_payment(required(payments, "california", "payments"), "payments.california");
  return result;
}

Household parse_household(json_t* root)
{
  require_object(root, {"tax_year", "filing_status", "residency", "spouses"}, "household");
  if (integer_value(root, "tax_year", "household") != 2026 ||
      string_value(root, "filing_status", "household") != "married_filing_jointly" ||
      string_value(root, "residency", "household") != "california_full_year") {
    throw ValidationError("household fixed values cannot be changed");
  }
  json_t* spouses = required(root, "spouses", "household");
  if (!json_is_array(spouses) || json_array_size(spouses) != 2) throw RequestShapeError("household.spouses must contain two spouses");
  Household result;
  bool seen_1{}, seen_2{};
  for (std::size_t index = 0; index < 2; ++index) {
    json_t* item = json_array_get(spouses, index);
    require_object(item, {"key", "label", "age_65_or_older", "blind"}, "household.spouses");
    const std::string key = string_value(item, "key", "household.spouses");
    Spouse spouse{key == "spouse_1" ? SpouseKey::spouse_1 : SpouseKey::spouse_2,
                  string_value(item, "label", "household.spouses"),
                  boolean_value(item, "age_65_or_older", "household.spouses"),
                  boolean_value(item, "blind", "household.spouses")};
    if (key == "spouse_1" && !seen_1) { result.spouse_1 = std::move(spouse); seen_1 = true; }
    else if (key == "spouse_2" && !seen_2) { result.spouse_2 = std::move(spouse); seen_2 = true; }
    else throw ValidationError("household spouse keys are invalid");
  }
  return result;
}

std::vector<Bracket> parse_brackets(json_t* object, const char* name, std::string_view path)
{
  json_t* values = required(object, name, path);
  if (!json_is_array(values)) throw RequestShapeError(std::string(path) + "." + name + " must be an array");
  std::vector<Bracket> result;
  std::size_t index{};
  json_t* item{};
  json_array_foreach(values, index, item) {
    require_object(item, {"lower_bound_cents", "upper_bound_cents", "rate_ppm"}, name);
    json_t* upper = required(item, "upper_bound_cents", name);
    if (!json_is_null(upper) && !json_is_integer(upper)) throw RequestShapeError("bracket upper bound must be an integer or null");
    result.push_back({integer_value(item, "lower_bound_cents", name), json_is_null(upper) ? std::nullopt : std::optional<Cents>{json_integer_value(upper)},
                      int_value(item, "rate_ppm", name)});
  }
  return result;
}

std::vector<Installment> parse_installments(json_t* object, std::string_view path)
{
  json_t* values = required(object, "installments", path);
  if (!json_is_array(values)) throw RequestShapeError("installments must be an array");
  std::vector<Installment> result;
  std::size_t index{};
  json_t* item{};
  json_array_foreach(values, index, item) {
    require_object(item, {"quarter", "due_date", "period_ppm", "cumulative_ppm"}, "installments");
    result.push_back({int_value(item, "quarter", "installments"),
                      string_value(item, "due_date", "installments"),
                      int_value(item, "period_ppm", "installments"),
                      int_value(item, "cumulative_ppm", "installments")});
  }
  return result;
}

FederalRules parse_federal_rules(json_t* value)
{
  require_object(value, {"tax_year", "standard_deduction_cents", "age_or_blind_addition_cents", "ordinary_brackets",
                         "preferential_brackets", "capital_loss_limit_cents", "additional_taxes", "installments"}, "federal");
  FederalRules rules;
  rules.tax_year = int_value(value, "tax_year", "federal");
  rules.standard_deduction_cents = integer_value(value, "standard_deduction_cents", "federal");
  rules.age_or_blind_addition_cents = integer_value(value, "age_or_blind_addition_cents", "federal");
  rules.ordinary_brackets = parse_brackets(value, "ordinary_brackets", "federal");
  rules.preferential_brackets = parse_brackets(value, "preferential_brackets", "federal");
  rules.capital_loss_limit_cents = integer_value(value, "capital_loss_limit_cents", "federal");
  json_t* additional = required(value, "additional_taxes", "federal");
  require_object(additional, {"niit", "additional_medicare"}, "federal.additional_taxes");
  json_t* niit = required(additional, "niit", "federal.additional_taxes");
  require_object(niit, {"threshold_cents", "rate_ppm"}, "federal.additional_taxes.niit");
  rules.niit_threshold_cents = integer_value(niit, "threshold_cents", "federal.additional_taxes.niit");
  rules.niit_rate_ppm = int_value(niit, "rate_ppm", "federal.additional_taxes.niit");
  json_t* medicare = required(additional, "additional_medicare", "federal.additional_taxes");
  require_object(medicare, {"threshold_cents", "rate_ppm"}, "federal.additional_taxes.additional_medicare");
  rules.additional_medicare_threshold_cents = integer_value(medicare, "threshold_cents", "federal.additional_taxes.additional_medicare");
  rules.additional_medicare_rate_ppm = int_value(medicare, "rate_ppm", "federal.additional_taxes.additional_medicare");
  rules.installments = parse_installments(value, "federal");
  return rules;
}

CaliforniaRules parse_california_rules(json_t* value)
{
  require_object(value, {"tax_year", "standard_deduction_cents", "ordinary_brackets", "joint_personal_exemption_credit_cents",
                         "capital_loss_limit_cents", "additional_taxes", "installments"}, "california");
  CaliforniaRules rules;
  rules.tax_year = int_value(value, "tax_year", "california");
  rules.standard_deduction_cents = integer_value(value, "standard_deduction_cents", "california");
  rules.ordinary_brackets = parse_brackets(value, "ordinary_brackets", "california");
  rules.joint_personal_exemption_credit_cents = integer_value(value, "joint_personal_exemption_credit_cents", "california");
  rules.capital_loss_limit_cents = integer_value(value, "capital_loss_limit_cents", "california");
  json_t* additional = required(value, "additional_taxes", "california");
  require_object(additional, {"behavioral_health_services"}, "california.additional_taxes");
  json_t* behavioral = required(additional, "behavioral_health_services", "california.additional_taxes");
  require_object(behavioral, {"threshold_cents", "rate_ppm"}, "california.additional_taxes.behavioral_health_services");
  rules.behavioral_health_services_threshold_cents = integer_value(behavioral, "threshold_cents", "california.additional_taxes.behavioral_health_services");
  rules.behavioral_health_services_rate_ppm = int_value(behavioral, "rate_ppm", "california.additional_taxes.behavioral_health_services");
  rules.installments = parse_installments(value, "california");
  return rules;
}

std::optional<int> quarter_path(std::string_view path)
{
  constexpr std::string_view prefix = "/api/2026/quarters/";
  if (!path.starts_with(prefix)) return std::nullopt;
  const std::string_view value = path.substr(prefix.size());
  if (value.size() != 1 || value[0] < '1' || value[0] > '4') return 0;
  return value[0] - '0';
}

std::optional<std::int64_t> parse_positive_id(std::string_view value)
{
  std::int64_t id{};
  const auto [end, error] = std::from_chars(value.data(), value.data() + value.size(), id);
  if (error != std::errc{} || end != value.data() + value.size() || id <= 0) return std::nullopt;
  return id;
}

struct SnapshotPath {
  std::optional<std::int64_t> id;
  bool metadata{};
};

std::optional<SnapshotPath> snapshot_path(std::string_view path)
{
  constexpr std::string_view prefix = "/api/2026/snapshots/";
  if (!path.starts_with(prefix)) return std::nullopt;
  std::string_view rest = path.substr(prefix.size());
  bool metadata{};
  constexpr std::string_view suffix = "/metadata";
  if (rest.ends_with(suffix)) { rest.remove_suffix(suffix.size()); metadata = true; }
  return SnapshotPath{parse_positive_id(rest), metadata};
}

json_t* snapshot_summary_json(const CalculationSnapshot& snapshot)
{
  json_t* value = json_object();
  json_object_set_new(value, "id", json_string(std::to_string(snapshot.id).c_str()));
  json_object_set_new(value, "label", json_string(snapshot.name.c_str()));
  json_object_set_new(value, "as_of_date", json_string(snapshot.as_of_date.c_str()));
  return value;
}

json_t* snapshot_json(const CalculationSnapshot& snapshot)
{
  json_t* value = snapshot_summary_json(snapshot);
  json_object_set_new(value, "household", household_json(snapshot.household));
  json_t* quarters = json_array();
  for (const auto& quarter : snapshot.inputs.quarters) json_array_append_new(quarters, quarter_input_json(quarter));
  json_object_set_new(value, "quarters", quarters);
  CurrentResult current{snapshot.household, snapshot.as_of_date, snapshot.projection, snapshot.taxes, snapshot.recommendations};
  json_object_set_new(value, "current_result", current_result_json(current));
  json_object_set_new(value, "rules", active_rules_json(snapshot.rules));
  return value;
}

std::optional<std::string_view> content_type(const ApiRequest& request)
{
  for (const auto& [name, value] : request.headers) {
    std::string lowered = name;
    std::transform(lowered.begin(), lowered.end(), lowered.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    if (lowered == "content-type") return value;
  }
  return std::nullopt;
}

bool has_json_content_type(const ApiRequest& request)
{
  const auto value = content_type(request);
  return value && value->starts_with("application/json");
}

bool has_restore_content_type(const ApiRequest& request)
{
  const auto value = content_type(request);
  return value && (value->starts_with("application/octet-stream") || value->starts_with("application/vnd.sqlite3"));
}

ApiResponse quarter_resource(const QuarterInput& input, const CurrentResult& result)
{
  json_t* value = json_object();
  json_object_set_new(value, "tax_year", json_integer(2026));
  json_object_set_new(value, "quarter", json_integer(input.quarter));
  json_object_set_new(value, "input", quarter_input_json(input));
  json_t* derived = json_object();
  json_object_set_new(derived, "as_of_date", json_string(result.as_of_date.c_str()));
  json_object_set_new(derived, "annual_summary", projection_json(result.projection));
  json_object_set_new(derived, "federal", tax_result_json(result.taxes.federal));
  json_object_set_new(derived, "california", tax_result_json(result.taxes.california));
  json_object_set_new(derived, "current_recommendations", recommendations_json(result.recommendations));
  json_object_set_new(value, "result", derived);
  json_object_set_new(value, "warnings", warnings_json(result.projection.warnings));
  return json_response(200, value);
}

}  // namespace

ApiApplication::ApiApplication(std::string database_path, const CurrentDateProvider& clock)
    : database_path_(std::move(database_path)), clock_(clock)
{
  InputStore inputs(database_path_);
  RuleStore rules(database_path_);
  SnapshotStore snapshots(database_path_);
  (void)inputs.load_household();
  (void)rules.load_active();
  (void)snapshots.list();
}

ApiResponse ApiApplication::handle(const ApiRequest& request) const
{
  try {
    const bool has_body = request.method == "PUT" || request.method == "POST";
    if (request.body.size() > (request.path == "/api/restore" ? http::kMaximumRestoreBodyBytes : http::kMaximumJsonBodyBytes)) {
      return error_response(413, "payload_too_large", "The request body is too large.");
    }
    if (has_body && request.path != "/api/restore" && !has_json_content_type(request)) {
      return error_response(415, "unsupported_content_type", "Content-Type must be application/json.");
    }
    if (request.method == "POST" && request.path == "/api/restore" && !has_restore_content_type(request)) {
      return error_response(415, "unsupported_content_type", "Content-Type must be a SQLite binary type.");
    }

    if (request.method == "GET" && request.path == "/api/2026/household") {
      InputStore store(database_path_);
      return json_response(200, household_json(store.load_household()));
    }
    if (request.method == "PUT" && request.path == "/api/2026/household") {
      const std::string as_of_date = clock_.current_date();
      Json body = parse_json(request.body);
      const Household household = parse_household(body.get());
      InputStore store(database_path_);
      RuleStore rules(database_path_);
      CurrentState state = load_current_state(store, rules);
      state.household = household;
      const CurrentResult result = compose(state, as_of_date);
      store.replace_household(household);
      json_t* response = json_object();
      json_object_set_new(response, "household", household_json(household));
      json_object_set_new(response, "current_result", current_result_json(result));
      return json_response(200, response);
    }
    if (request.method == "GET" && request.path == "/api/2026") {
      const std::string as_of_date = clock_.current_date();
      InputStore store(database_path_);
      RuleStore rules(database_path_);
      const CurrentState state = load_current_state(store, rules);
      return json_response(200, bootstrap_json(compose(state, as_of_date), state.inputs));
    }

    if (const auto quarter = quarter_path(request.path)) {
      if (*quarter == 0) return error_response(404, "not_found", "The requested resource was not found.");
      const std::string as_of_date = clock_.current_date();
      InputStore store(database_path_);
      RuleStore rules(database_path_);
      CurrentState state = load_current_state(store, rules);
      if (request.method == "GET") return quarter_resource(state.inputs.quarters[*quarter - 1], compose(state, as_of_date));
      if (request.method == "PUT") {
        Json body = parse_json(request.body);
        const QuarterInput input = parse_quarter(body.get(), *quarter);
        state.inputs.quarters[*quarter - 1] = input;
        const CurrentResult result = compose(state, as_of_date);
        store.replace_quarter(input);
        return quarter_resource(input, result);
      }
    }

    if (request.method == "GET" && request.path == "/api/2026/tax-rules") {
      RuleStore store(database_path_);
      return json_response(200, rule_resource_json(store.load_active(), store));
    }
    if (request.method == "PUT" && request.path == "/api/2026/tax-rules") {
      const std::string as_of_date = clock_.current_date();
      Json body = parse_json(request.body);
      require_object(body.get(), {"federal", "california"}, "tax_rules");
      const FederalRules federal = parse_federal_rules(required(body.get(), "federal", "tax_rules"));
      const CaliforniaRules california = parse_california_rules(required(body.get(), "california", "tax_rules"));
      validate(federal);
      validate(california);
      InputStore inputs(database_path_);
      RuleStore store(database_path_);
      CurrentState state = load_current_state(inputs, store);
      state.rules.federal.federal = federal;
      state.rules.california.california = california;
      (void)compose(state, as_of_date);
      store.replace_active(federal, california);
      state.rules = store.load_active();
      const CurrentResult result = compose(state, as_of_date);
      json_t* response = json_object();
      json_object_set_new(response, "rules", rule_resource_json(state.rules, store));
      json_object_set_new(response, "current_result", current_result_json(result));
      return json_response(200, response);
    }
    if (request.method == "POST" && request.path == "/api/2026/tax-rules/restore") {
      const std::string as_of_date = clock_.current_date();
      Json body = parse_json(request.body);
      require_object(body.get(), {"jurisdiction", "source", "revision_id"}, "restore_rules");
      const std::string jurisdiction_value = string_value(body.get(), "jurisdiction", "restore_rules");
      const std::string source = string_value(body.get(), "source", "restore_rules");
      if (jurisdiction_value != "federal" && jurisdiction_value != "california") throw ValidationError("jurisdiction is invalid");
      const Jurisdiction jurisdiction = jurisdiction_value == "federal" ? Jurisdiction::federal : Jurisdiction::california;
      RuleStore store(database_path_);
      if (source == "official") {
        if (json_object_get(body.get(), "revision_id") != nullptr) throw RequestShapeError("revision_id is not allowed for official restore");
        store.restore_official(jurisdiction);
      } else if (source == "revision") {
        const std::string id_value = string_value(body.get(), "revision_id", "restore_rules");
        const auto id = parse_positive_id(id_value);
        if (!id) throw RequestShapeError("revision_id is invalid");
        store.restore_archived(jurisdiction, *id);
      } else {
        throw ValidationError("restore source is invalid");
      }
      InputStore inputs(database_path_);
      const CurrentState state = load_current_state(inputs, store);
      const CurrentResult result = compose(state, as_of_date);
      json_t* response = json_object();
      json_object_set_new(response, "rules", rule_resource_json(state.rules, store));
      json_object_set_new(response, "current_result", current_result_json(result));
      return json_response(200, response);
    }

    if (request.path == "/api/2026/snapshots") {
      SnapshotStore snapshots(database_path_);
      if (request.method == "GET") {
        auto values = snapshots.list();
        json_t* response = json_array();
        for (auto iterator = values.rbegin(); iterator != values.rend(); ++iterator) json_array_append_new(response, snapshot_summary_json(*iterator));
        return json_response(200, response);
      }
      if (request.method == "POST") {
        const std::string as_of_date = clock_.current_date();
        Json body = parse_json(request.body);
        require_object(body.get(), {"label"}, "snapshot");
        CalculationSnapshot snapshot;
        snapshot.name = string_value(body.get(), "label", "snapshot");
        snapshot.as_of_date = as_of_date;
        InputStore inputs(database_path_);
        RuleStore rules(database_path_);
        CurrentState state = load_current_state(inputs, rules);
        snapshot.household = std::move(state.household);
        snapshot.inputs = std::move(state.inputs);
        snapshot.rules = std::move(state.rules);
        const CalculationSnapshot saved = snapshots.save(std::move(snapshot));
        return json_response(201, snapshot_summary_json(saved));
      }
    }
    if (const auto parsed = snapshot_path(request.path)) {
      if (!parsed->id) return error_response(404, "not_found", "The requested resource was not found.");
      SnapshotStore snapshots(database_path_);
      if (request.method == "GET" && !parsed->metadata) return json_response(200, snapshot_json(snapshots.load(*parsed->id)));
      if (request.method == "PUT" && parsed->metadata) {
        Json body = parse_json(request.body);
        require_object(body.get(), {"label"}, "snapshot_metadata");
        snapshots.rename(*parsed->id, string_value(body.get(), "label", "snapshot_metadata"));
        return json_response(200, snapshot_summary_json(snapshots.load(*parsed->id)));
      }
      if (request.method == "DELETE" && !parsed->metadata) {
        snapshots.remove(*parsed->id);
        return {204, {}, {}};
      }
    }

    if (request.method == "GET" && request.path == "/api/backup") {
      return {200, {{"Content-Type", "application/vnd.sqlite3"},
                    {"Content-Disposition", "attachment; filename=estimated-taxes-2026.sqlite"}},
              backup_database(database_path_)};
    }
    if (request.method == "POST" && request.path == "/api/restore") {
      const std::string as_of_date = clock_.current_date();
      restore_database(database_path_, request.body, as_of_date);
      InputStore inputs(database_path_);
      RuleStore rules(database_path_);
      const CurrentState state = load_current_state(inputs, rules);
      return json_response(200, bootstrap_json(compose(state, as_of_date), state.inputs));
    }

    return error_response(404, "not_found", "The requested resource was not found.");
  } catch (const RequestShapeError&) {
    return error_response(400, "invalid_request", "The request shape is invalid.");
  } catch (const RuleValidationError& error) {
    return validation_error_response(error.what());
  } catch (const ValidationError& error) {
    return validation_error_response(error.what(), error.path(), error.code());
  } catch (const ProjectionError& error) {
    return validation_error_response(error.what());
  } catch (const RecommendationError& error) {
    return validation_error_response(error.what());
  } catch (const CalculationError& error) {
    return validation_error_response(error.what());
  } catch (const StorageError& error) {
    if (std::string_view(error.what()).find("not found") != std::string_view::npos) {
      return error_response(404, "not_found", "The requested resource was not found.");
    }
    return error_response(500, "internal_error", "The request could not be completed.");
  } catch (...) {
    return error_response(500, "internal_error", "The request could not be completed.");
  }
}

}  // namespace estimated_taxes
