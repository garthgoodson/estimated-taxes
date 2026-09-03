#include "estimated_taxes/tax_calculation.hpp"

#include <array>
#include <functional>
#include <iostream>
#include <stdexcept>

using namespace estimated_taxes;
namespace {
void require(bool condition, const char* message) { if (!condition) throw std::runtime_error(message); }
struct Boundary { Cents bound; Cents tax; };
ActiveRules active_rules() { return {{1, Jurisdiction::federal, false, false, {}, official_federal_rules(), {}}, {2, Jurisdiction::california, false, false, {}, {}, official_california_rules()}, false, false}; }
AnnualProjection projection(Cents federal = 0, Cents california = 0) { AnnualProjection value; value.federal_wages.projected_annual_cents = federal; value.california_wages.projected_annual_cents = california; return value; }

void test_rate_rounding_and_capital_netting() {
  require(multiply_rate(5, 100000) == 1 && multiply_rate(-5, 100000) == -1, "half cents round away from zero");
  require(multiply_rate(4, 125000) == 1 && multiply_rate(-4, 125000) == -1, "half-cent boundaries");
  const std::array<std::array<Cents, 5>, 6> cases{{{100, 200, 100, 200, 0}, {-100, -200, 0, 0, 200}, {300, -100, 200, 0, 0}, {100, -300, 0, 0, 200}, {-100, 300, 0, 200, 0}, {-300, 100, 0, 0, 200}}};
  for (const auto& c : cases) { const auto net = net_capital_gains(c[0], c[1], 200); require(net.ordinary_gain_cents == c[2] && net.preferential_gain_cents == c[3] && net.deductible_loss_cents == c[4], "capital netting case"); }
  for (const Cents loss : {Cents{199}, Cents{200}, Cents{201}}) { const auto net=net_capital_gains(-loss, 0, 200); require(net.deductible_loss_cents == std::min(loss, Cents{200}) && net.unused_loss_cents == std::max(Cents{0}, loss-200), "loss limit boundary"); }
}

void test_official_ordinary_bracket_boundaries() {
  struct Case { Cents bound; Cents tax; };
  constexpr std::array<Boundary, 6> federal{{{2480000,248000},{10080000,1160000},{21140000,3593200},{40355000,8204800},{51245000,11689600},{76870000,20658350}}};
  constexpr std::array<Boundary, 8> california{{{2215800,22158},{5252800,82898},{8290400,204402},{11508400,397482},{14544800,640394},{74295800,6197237},{89154200,7727652},{148590600,14443965}}};
  const auto fr=official_federal_rules(); const auto cr=official_california_rules();
  for (const auto& c : federal) { require(progressive_tax(c.bound,fr.ordinary_brackets)==c.tax, "federal official boundary fixture"); require(progressive_tax(c.bound-1,fr.ordinary_brackets)<=c.tax && progressive_tax(c.bound+1,fr.ordinary_brackets)>=c.tax, "federal exclusive boundary"); }
  for (const auto& c : california) { require(progressive_tax(c.bound,cr.ordinary_brackets)==c.tax, "California official boundary fixture"); require(progressive_tax(c.bound-1,cr.ordinary_brackets)<=c.tax && progressive_tax(c.bound+1,cr.ordinary_brackets)>=c.tax, "California exclusive boundary"); }
}

void test_federal_preferential_and_threshold_boundaries() {
  Household household; auto rules=active_rules();
  for (const Cents bound : {Cents{9890000}, Cents{61370000}}) for (const Cents delta : {Cents{-1}, Cents{0}, Cents{1}}) { const Cents preferential=bound+delta; auto p=projection(3220000 + 1000000, 0); p.investments.ordinary_dividends_cents=preferential; p.investments.qualified_dividends_cents=preferential; const auto result=calculate_tax(p,household,rules).federal; require(result.details.preferential_tax_cents <= result.details.regular_tax_cents, "preferential stacked boundary path"); }
  for (const Cents wage : {Cents{23999999}, Cents{24000000}, Cents{24000001}}) { auto p=projection(wage, 0); p.investments={1000000,0,0,0,0,0}; const auto r=calculate_tax(p,household,rules).federal; require(r.details.niit_cents==0, "NIIT one-cent threshold rounding"); }
  auto niit=projection(24000027,0); niit.investments={1000000,0,0,0,0,0}; require(calculate_tax(niit,household,rules).federal.details.niit_cents==1, "NIIT applies above rounding boundary");
  for (const Cents wage : {Cents{24999999}, Cents{25000000}, Cents{25000001}}) { const auto r=calculate_tax(projection(wage,0),household,rules).federal; require(r.details.additional_medicare_cents==0, "Additional Medicare one-cent threshold rounding"); }
  require(calculate_tax(projection(25000056,0),household,rules).federal.details.additional_medicare_cents==1, "Additional Medicare applies above rounding boundary");
}

void test_dividends_withholding_custom_and_reconciliation() {
  Household household; auto rules=active_rules(); auto p=projection(10000000, 10000000); p.investments={100000,100000,0,0,7,11}; p.federal_withholding.projected_annual_cents=13; p.california_withholding.projected_annual_cents=17;
  auto result=calculate_tax(p,household,rules); const auto& f=result.federal.details; const auto& c=result.california.details;
  require(f.supported_agi_cents==10100000 && f.projected_withholding_cents==20 && c.projected_withholding_cents==28, "dividends are not double counted and withholding is jurisdictional");
  require(f.ordinary_taxable_income_cents+f.preferential_pool_cents==f.taxable_income_cents && f.preferential_pool_cents<=f.taxable_income_cents, "federal income allocation reconciliation");
  require(f.regular_tax_cents==std::min(f.ordinary_tax_on_all_income_cents,f.ordinary_tax_cents+f.preferential_tax_cents), "federal lesser-of reconciliation");
  require(c.tax_before_exemption_cents==c.ordinary_tax_cents && c.tax_after_exemption_cents==std::max(Cents{0},c.tax_before_exemption_cents-c.exemption_credit_cents), "California exemption reconciliation");
  require(f.annual_liability_cents==f.regular_tax_cents+f.niit_cents+f.additional_medicare_cents && c.annual_liability_cents==c.tax_after_exemption_cents+c.behavioral_health_services_cents, "liability reconciliation");
  require(f.amount_requiring_estimated_payments_cents==std::max(Cents{0},f.annual_liability_cents-f.projected_withholding_cents) && c.amount_requiring_estimated_payments_cents==std::max(Cents{0},c.annual_liability_cents-c.projected_withholding_cents), "payment balance reconciliation");
  p.federal_withholding.projected_annual_cents=f.annual_liability_cents-7; require(calculate_tax(p,household,rules).federal.details.amount_requiring_estimated_payments_cents==0, "withholding state");
  rules.california.modified=true; rules.california.california->standard_deduction_cents=0; const auto custom=calculate_tax(p,household,rules).california; require(custom.details.taxable_income_cents==10100000 && !custom.warnings.empty(), "custom California rules are consumed");
  auto federal_rules=active_rules(); federal_rules.federal.modified=true; federal_rules.federal.federal->standard_deduction_cents=0; const auto custom_federal=calculate_tax(p,household,federal_rules).federal; require(custom_federal.details.taxable_income_cents==10100000 && !custom_federal.warnings.empty(), "custom federal rules are consumed");
}

void test_california_credit_bhs_and_official_fixtures() {
  Household household; auto rules=active_rules();
  auto zero=calculate_tax(projection(),household,rules); require(zero.federal.details.annual_liability_cents==0 && zero.california.details.annual_liability_cents==0, "zero-income fixture");
  auto federal=calculate_tax(projection(4220000,0),household,rules).federal; require(federal.details.taxable_income_cents==1000000 && federal.details.ordinary_taxable_income_cents==1000000 && federal.details.preferential_pool_cents==0 && federal.details.ordinary_tax_on_all_income_cents==100000 && federal.details.annual_liability_cents==100000, "federal fixture: $42,200 wages minus $32,200 deduction, 10% tax");
  auto california=calculate_tax(projection(0,12141200),household,rules).california; require(california.details.taxable_income_cents==11000000 && california.details.tax_before_exemption_cents==366978 && california.details.tax_after_exemption_cents==336378 && california.details.annual_liability_cents==336378, "California fixture: Schedule Y tax less $306 credit");
  for (const Cents taxable : {Cents{99999999}, Cents{100000000}, Cents{100000001}}) { auto r=calculate_tax(projection(0,taxable+1141200),household,rules).california; require(r.details.behavioral_health_services_cents==0, "BHS one-cent threshold rounding"); }
  require(calculate_tax(projection(0,100000050+1141200),household,rules).california.details.behavioral_health_services_cents==1, "BHS applies above rounding boundary");
  const auto credit=calculate_tax(projection(0,1141201),household,rules).california; require(credit.details.tax_before_exemption_cents>=0 && credit.details.tax_after_exemption_cents==0 && credit.details.regular_tax_cents==0, "California credit cannot make tax negative");
}
}
int main() { std::pair<const char*,std::function<void()>> tests[]={{"rounding and capital netting",test_rate_rounding_and_capital_netting},{"ordinary bracket boundaries",test_official_ordinary_bracket_boundaries},{"federal preferential and thresholds",test_federal_preferential_and_threshold_boundaries},{"dividends withholding custom reconciliation",test_dividends_withholding_custom_and_reconciliation},{"California credit BHS fixtures",test_california_credit_bhs_and_official_fixtures}}; int failed{}; for(auto& [name,test]:tests) try{test();std::cout<<"PASS: "<<name<<'\n';}catch(const std::exception& e){++failed;std::cerr<<"FAIL: "<<name<<": "<<e.what()<<'\n';} return failed; }
