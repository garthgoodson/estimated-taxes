#include "estimated_taxes/api.hpp"

#include <jansson.h>

#include <filesystem>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>

using namespace estimated_taxes;
namespace {

class FixedClock final : public CurrentDateProvider {
public:
  explicit FixedClock(std::string date) : date_(std::move(date)) {}
  [[nodiscard]] std::string current_date() const override { ++calls; return date_; }
  mutable int calls{};
private:
  std::string date_;
};

class Json {
public:
  explicit Json(const std::string& text)
  {
    json_error_t error{};
    value_ = json_loadb(text.data(), text.size(), JSON_REJECT_DUPLICATES, &error);
    if (value_ == nullptr) throw std::runtime_error("response is not valid JSON");
  }
  explicit Json(json_t* value) : value_(value) {}
  ~Json() { json_decref(value_); }
  Json(const Json&) = delete;
  Json& operator=(const Json&) = delete;
  [[nodiscard]] json_t* get() const { return value_; }
private:
  json_t* value_{};
};

void require(bool condition, const char* message)
{
  if (!condition) throw std::runtime_error(message);
}

std::string database_path()
{
  static int sequence{};
  const auto path = std::filesystem::temp_directory_path() /
                    ("estimated_taxes_api_test_" + std::to_string(++sequence) + ".sqlite");
  std::filesystem::remove(path);
  return path.string();
}

ApiRequest json_request(std::string method, std::string path, std::string body)
{
  return {std::move(method), std::move(path), {{"Content-Type", "application/json"}}, std::move(body)};
}

std::string empty_quarter(std::string investments = "null")
{
  return "{\"paystubs\":{\"spouse_1\":null,\"spouse_2\":null},\"investments\":" + investments +
         ",\"payments\":{\"federal\":{\"amount_cents\":0,\"date\":null},"
         "\"california\":{\"amount_cents\":0,\"date\":null}}}";
}

json_t* member(json_t* object, const char* name)
{
  json_t* value = json_object_get(object, name);
  if (value == nullptr) throw std::runtime_error(std::string("missing JSON member: ") + name);
  return value;
}

void bootstrap_and_complete_serialization()
{
  const std::string path = database_path();
  FixedClock clock("2026-03-31");
  ApiApplication app(path, clock);
  const ApiResponse bootstrap = app.handle({"GET", "/api/2026", {}, {}});
  require(bootstrap.status == 200 && clock.calls == 1, "bootstrap resolves fixed clock once");
  Json bootstrap_json(bootstrap.body);
  require(json_integer_value(member(bootstrap_json.get(), "tax_year")) == 2026, "bootstrap tax year");
  require(json_array_size(member(bootstrap_json.get(), "quarters")) == 4, "bootstrap quarter summaries");

  const ApiResponse quarter = app.handle({"GET", "/api/2026/quarters/1", {}, {}});
  require(quarter.status == 200 && clock.calls == 2, "quarter read resolves clock once");
  Json quarter_json(quarter.body);
  json_t* result = member(quarter_json.get(), "result");
  json_t* projection = member(result, "annual_summary");
  require(json_object_get(projection, "spouses") != nullptr && json_object_get(projection, "investments") != nullptr,
          "full projection serialized");
  json_t* federal_tax = member(result, "federal");
  require(json_object_get(member(federal_tax, "details"), "amount_requiring_estimated_payments_cents") != nullptr &&
              json_object_get(federal_tax, "capital_netting") != nullptr,
          "full tax result serialized");
  json_t* federal_recommendation = member(member(result, "current_recommendations"), "federal");
  require(json_object_get(federal_recommendation, "future_outlook") != nullptr &&
              json_object_get(federal_recommendation, "remaining_after_recommendation_cents") != nullptr,
          "full recommendation serialized");
  std::filesystem::remove(path);
}

void quarter_strictness_validation_and_rollback()
{
  const std::string path = database_path();
  FixedClock clock("2026-03-31");
  ApiApplication app(path, clock);
  const std::string investment =
      "{\"ordinary_dividends_cents\":100,\"qualified_dividends_cents\":50,\"short_term_gain_cents\":-25,"
      "\"long_term_gain_cents\":200,\"federal_withholding_cents\":3,\"california_withholding_cents\":2,\"notes\":null}";
  require(app.handle(json_request("PUT", "/api/2026/quarters/1", empty_quarter(investment))).status == 200,
          "valid quarter saved");

  const std::string unknown =
      "{\"ordinary_dividends_cents\":999,\"qualified_dividends_cents\":50,\"short_term_gain_cents\":0,"
      "\"long_term_gain_cents\":0,\"federal_withholding_cents\":0,\"california_withholding_cents\":0,"
      "\"notes\":null,\"unexpected\":1}";
  require(app.handle(json_request("PUT", "/api/2026/quarters/1", empty_quarter(unknown))).status == 400,
          "nested unknown field rejected");
  const std::string invalid =
      "{\"ordinary_dividends_cents\":10,\"qualified_dividends_cents\":11,\"short_term_gain_cents\":0,"
      "\"long_term_gain_cents\":0,\"federal_withholding_cents\":0,\"california_withholding_cents\":0,\"notes\":null}";
  const ApiResponse validation = app.handle(json_request("PUT", "/api/2026/quarters/1", empty_quarter(invalid)));
  require(validation.status == 422 && validation.body.find("investments.qualified_dividends_cents") != std::string::npos &&
              validation.body.find("exceeds_ordinary_dividends") != std::string::npos,
          "domain validation returns field details");
  const ApiResponse saved = app.handle({"GET", "/api/2026/quarters/1", {}, {}});
  require(saved.body.find("\"ordinary_dividends_cents\":100") != std::string::npos,
          "failed saves preserve prior quarter");
  require(app.handle(json_request("PUT", "/api/2026/quarters/1", "{}")).status == 400,
          "missing fields rejected");
  require(app.handle(json_request("PUT", "/api/2026/quarters/4", empty_quarter(investment))).status == 422,
          "future-quarter facts rejected");
  require(app.handle({"PUT", "/api/2026/quarters/1", {}, empty_quarter()}).status == 415,
          "JSON content type required");
  std::filesystem::remove(path);
}

void household_rules_and_snapshots()
{
  const std::string path = database_path();
  FixedClock clock("2026-03-31");
  ApiApplication app(path, clock);
  const std::string household =
      "{\"tax_year\":2026,\"filing_status\":\"married_filing_jointly\",\"residency\":\"california_full_year\","
      "\"spouses\":[{\"key\":\"spouse_1\",\"label\":\"Alex\",\"age_65_or_older\":false,\"blind\":false},"
      "{\"key\":\"spouse_2\",\"label\":\"Sam\",\"age_65_or_older\":true,\"blind\":false}]}";
  require(app.handle(json_request("PUT", "/api/2026/household", household)).status == 200, "household replaced");

  const ApiResponse rules_response = app.handle({"GET", "/api/2026/tax-rules", {}, {}});
  require(rules_response.status == 200, "rules loaded");
  Json rules(rules_response.body);
  require(json_array_size(member(member(rules.get(), "federal"), "sources")) > 0, "rule sources serialized");
  require(json_is_object(member(member(member(rules.get(), "federal"), "additional_taxes"), "niit")),
          "NIIT serialized as object");
  json_t* federal_editable = json_deep_copy(member(rules.get(), "federal"));
  json_t* california_editable = json_deep_copy(member(rules.get(), "california"));
  for (json_t* jurisdiction : {federal_editable, california_editable}) {
    json_object_del(jurisdiction, "revision_id");
    json_object_del(jurisdiction, "official");
    json_object_del(jurisdiction, "sources");
    json_object_del(jurisdiction, "archived_revisions");
  }
  json_t* replacement = json_object();
  json_object_set_new(replacement, "federal", federal_editable);
  json_object_set_new(replacement, "california", california_editable);
  char* encoded = json_dumps(replacement, JSON_COMPACT);
  const std::string replacement_body(encoded);
  free(encoded);
  json_decref(replacement);
  const ApiResponse replaced = app.handle(json_request("PUT", "/api/2026/tax-rules", replacement_body));
  require(replaced.status == 200 && replaced.body.find("\"current_result\"") != std::string::npos,
          "rules replaced with refreshed result");
  Json replaced_json(replaced.body);
  const std::string active_federal_id = json_string_value(member(member(member(replaced_json.get(), "rules"), "federal"), "revision_id"));
  Json invalid_rules(replacement_body);
  json_object_set_new(member(invalid_rules.get(), "california"), "standard_deduction_cents", json_integer(-1));
  char* invalid_encoded = json_dumps(invalid_rules.get(), JSON_COMPACT);
  const std::string invalid_body(invalid_encoded);
  free(invalid_encoded);
  require(app.handle(json_request("PUT", "/api/2026/tax-rules", invalid_body)).status == 422,
          "invalid jurisdiction rejects whole rule save");
  Json after_invalid(app.handle({"GET", "/api/2026/tax-rules", {}, {}}).body);
  require(active_federal_id == json_string_value(member(member(after_invalid.get(), "federal"), "revision_id")),
          "failed rule save changes neither jurisdiction");
  require(app.handle(json_request("POST", "/api/2026/tax-rules/restore",
                                  "{\"jurisdiction\":\"federal\",\"source\":\"official\"}"))
              .status == 200,
          "official rules restored");

  const ApiResponse created = app.handle(json_request("POST", "/api/2026/snapshots", "{\"label\":\"Initial\"}"));
  require(created.status == 201, "snapshot created");
  Json created_json(created.body);
  const std::string id = json_string_value(member(created_json.get(), "id"));
  require(app.handle({"GET", "/api/2026/snapshots", {}, {}}).body.find("Initial") != std::string::npos,
          "snapshot listed");
  const ApiResponse snapshot = app.handle({"GET", "/api/2026/snapshots/" + id, {}, {}});
  require(snapshot.status == 200 && snapshot.body.find("\"current_result\"") != std::string::npos &&
              snapshot.body.find("\"rules\"") != std::string::npos,
          "complete snapshot serialized");
  require(app.handle(json_request("PUT", "/api/2026/snapshots/" + id + "/metadata", "{\"label\":\"Renamed\"}"))
              .status == 200,
          "snapshot renamed");
  require(app.handle({"DELETE", "/api/2026/snapshots/" + id, {}, {}}).status == 204, "snapshot deleted");
  require(app.handle({"GET", "/api/2026/snapshots/" + id, {}, {}}).status == 404, "deleted snapshot missing");
  std::filesystem::remove(path);
}

void backup_restore_and_failed_restore_atomicity()
{
  const std::string path = database_path();
  FixedClock clock("2026-03-31");
  ApiApplication app(path, clock);
  const std::string original =
      "{\"tax_year\":2026,\"filing_status\":\"married_filing_jointly\",\"residency\":\"california_full_year\","
      "\"spouses\":[{\"key\":\"spouse_1\",\"label\":\"Before\",\"age_65_or_older\":false,\"blind\":false},"
      "{\"key\":\"spouse_2\",\"label\":\"Spouse 2\",\"age_65_or_older\":false,\"blind\":false}]}";
  require(app.handle(json_request("PUT", "/api/2026/household", original)).status == 200, "prepare backup state");
  const ApiResponse backup = app.handle({"GET", "/api/backup", {}, {}});
  require(backup.status == 200 && backup.body.starts_with("SQLite format 3"), "complete SQLite backup returned");

  std::string changed = original;
  changed.replace(changed.find("Before"), 6, "After");
  require(app.handle(json_request("PUT", "/api/2026/household", changed)).status == 200, "change state after backup");
  const ApiResponse restored = app.handle({"POST", "/api/restore", {{"Content-Type", "application/vnd.sqlite3"}}, backup.body});
  require(restored.status == 200, "backup restored");
  require(app.handle({"GET", "/api/2026/household", {}, {}}).body.find("Before") != std::string::npos,
          "restore fully replaced current data");

  const ApiResponse invalid = app.handle({"POST", "/api/restore", {{"Content-Type", "application/octet-stream"}}, "not sqlite"});
  require(invalid.status == 422, "invalid restore rejected");
  require(app.handle({"GET", "/api/2026/household", {}, {}}).body.find("Before") != std::string::npos,
          "failed restore preserves current data");
  std::filesystem::remove(path);
}

}  // namespace

int main()
{
  const std::pair<const char*, std::function<void()>> tests[] = {
      {"bootstrap serialization", bootstrap_and_complete_serialization},
      {"quarter strictness", quarter_strictness_validation_and_rollback},
      {"resources", household_rules_and_snapshots},
      {"backup restore", backup_restore_and_failed_restore_atomicity},
  };
  int failures{};
  for (const auto& [name, test] : tests) {
    try {
      test();
      std::cout << "PASS: " << name << '\n';
    } catch (const std::exception& error) {
      ++failures;
      std::cerr << "FAIL: " << name << ": " << error.what() << '\n';
    }
  }
  return failures;
}
