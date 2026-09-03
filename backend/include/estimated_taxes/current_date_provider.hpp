#pragma once

#include <string>

namespace estimated_taxes {

class CurrentDateProvider {
public:
  virtual ~CurrentDateProvider() = default;
  [[nodiscard]] virtual std::string current_date() const = 0;
};

class LocalCurrentDateProvider final : public CurrentDateProvider {
public:
  [[nodiscard]] std::string current_date() const override;
};

}  // namespace estimated_taxes
