#pragma once

#include "estimated_taxes/recommendations.hpp"

#include <memory>

namespace estimated_taxes {

struct CalculationSnapshot {
  std::int64_t id{};
  std::string name;
  std::string as_of_date;
  Household household;
  TaxYearInputs inputs;
  AnnualProjection projection;
  ActiveRules rules;
  TaxCalculation taxes;
  RecommendationResult recommendations;
};

class SnapshotStore {
public:
  explicit SnapshotStore(const std::string& database_path);
  ~SnapshotStore();
  SnapshotStore(const SnapshotStore&) = delete;
  SnapshotStore& operator=(const SnapshotStore&) = delete;

  [[nodiscard]] CalculationSnapshot save(CalculationSnapshot snapshot);
  [[nodiscard]] CalculationSnapshot load(std::int64_t id) const;
  [[nodiscard]] std::vector<CalculationSnapshot> list() const;
  void rename(std::int64_t id, const std::string& name);
  void remove(std::int64_t id);
private:
  struct Connection;
  std::unique_ptr<Connection> connection_;
};

}  // namespace estimated_taxes
