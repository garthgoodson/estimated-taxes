#pragma once

#include "estimated_taxes/input_store.hpp"

#include <optional>
#include <string>
#include <vector>

namespace estimated_taxes {

enum class Jurisdiction { federal, california };

struct Bracket {
  Cents lower_bound_cents{};
  std::optional<Cents> upper_bound_cents;
  int rate_ppm{};
  bool operator==(const Bracket&) const = default;
};

struct Installment {
  int quarter{};
  std::string due_date;
  int period_ppm{};
  int cumulative_ppm{};
  bool operator==(const Installment&) const = default;
};

struct Source {
  std::string agency;
  std::string title;
  std::string url;
  std::string publication_date;
  std::string verification_date;
  std::string interpretation_note;
  bool operator==(const Source&) const = default;
};

struct FederalRules {
  int tax_year{2026};
  Cents standard_deduction_cents{};
  Cents age_or_blind_addition_cents{};
  std::vector<Bracket> ordinary_brackets;
  std::vector<Bracket> preferential_brackets;
  Cents capital_loss_limit_cents{};
  Cents niit_threshold_cents{};
  int niit_rate_ppm{};
  Cents additional_medicare_threshold_cents{};
  int additional_medicare_rate_ppm{};
  std::vector<Installment> installments;
  bool operator==(const FederalRules&) const = default;
};

struct CaliforniaRules {
  int tax_year{2026};
  Cents standard_deduction_cents{};
  std::vector<Bracket> ordinary_brackets;
  Cents joint_personal_exemption_credit_cents{};
  Cents capital_loss_limit_cents{};
  Cents behavioral_health_services_threshold_cents{};
  int behavioral_health_services_rate_ppm{};
  std::vector<Installment> installments;
  bool operator==(const CaliforniaRules&) const = default;
};

struct RuleRevision {
  std::int64_t id{};
  Jurisdiction jurisdiction{};
  bool official_baseline{};
  bool modified{};
  std::vector<Source> sources;
  std::optional<FederalRules> federal;
  std::optional<CaliforniaRules> california;
};

struct ActiveRules {
  RuleRevision federal;
  RuleRevision california;
  bool federal_customized{};
  bool california_customized{};
};

class RuleValidationError : public std::runtime_error { public: using std::runtime_error::runtime_error; };

void validate(const FederalRules& rules);
void validate(const CaliforniaRules& rules);
[[nodiscard]] FederalRules official_federal_rules();
[[nodiscard]] CaliforniaRules official_california_rules();
[[nodiscard]] std::vector<Source> official_federal_sources();
[[nodiscard]] std::vector<Source> official_california_sources();

class RuleStore {
public:
  explicit RuleStore(const std::string& database_path);
  ~RuleStore();
  RuleStore(const RuleStore&) = delete;
  RuleStore& operator=(const RuleStore&) = delete;

  [[nodiscard]] ActiveRules load_active() const;
  void replace_active(const FederalRules& federal, const CaliforniaRules& california);
  void restore_official(Jurisdiction jurisdiction);
  void restore_archived(Jurisdiction jurisdiction, std::int64_t revision_id);
  [[nodiscard]] std::vector<RuleRevision> archived_revisions(Jurisdiction jurisdiction) const;

private:
  struct Connection;
  std::unique_ptr<Connection> connection_;
};

}  // namespace estimated_taxes
