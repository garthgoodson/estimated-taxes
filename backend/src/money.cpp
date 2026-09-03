#include "estimated_taxes/money.hpp"

#include "estimated_taxes/tax_calculation.hpp"

#include <limits>

namespace estimated_taxes {

Cents checked_add(Cents left, Cents right)
{
  if ((right > 0 && left > std::numeric_limits<Cents>::max() - right) ||
      (right < 0 && left < std::numeric_limits<Cents>::min() - right)) {
    throw CalculationError("monetary calculation overflow");
  }
  return left + right;
}

Cents checked_subtract(Cents left, Cents right)
{
  if (right == std::numeric_limits<Cents>::min()) throw CalculationError("monetary calculation overflow");
  return checked_add(left, -right);
}

Cents checked_multiply(Cents amount, int multiplier)
{
  if (multiplier != 0 && (amount > std::numeric_limits<Cents>::max() / multiplier ||
                          amount < std::numeric_limits<Cents>::min() / multiplier)) {
    throw CalculationError("monetary calculation overflow");
  }
  return amount * multiplier;
}

Cents divide_round_nearest(Cents numerator, Cents denominator)
{
  if (denominator <= 0) throw CalculationError("invalid division denominator");
  const Cents quotient = numerator / denominator;
  const Cents remainder = numerator % denominator;
  if (remainder == 0) return quotient;
  const Cents magnitude = remainder < 0 ? -remainder : remainder;
  if (magnitude < (denominator + 1) / 2) return quotient;
  return numerator < 0 ? checked_subtract(quotient, 1) : checked_add(quotient, 1);
}

}  // namespace estimated_taxes
