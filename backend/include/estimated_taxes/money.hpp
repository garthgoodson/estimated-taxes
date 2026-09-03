#pragma once

#include "estimated_taxes/input_store.hpp"

namespace estimated_taxes {

[[nodiscard]] Cents checked_add(Cents left, Cents right);
[[nodiscard]] Cents checked_subtract(Cents left, Cents right);
[[nodiscard]] Cents checked_multiply(Cents amount, int multiplier);
[[nodiscard]] Cents divide_round_nearest(Cents numerator, Cents denominator);

}  // namespace estimated_taxes
