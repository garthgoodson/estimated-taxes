#include "estimated_taxes/current_date_provider.hpp"

#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>

namespace estimated_taxes {

std::string LocalCurrentDateProvider::current_date() const
{
  const std::time_t now = std::time(nullptr);
  std::tm local{};
#if defined(_WIN32)
  localtime_s(&local, &now);
#else
  localtime_r(&now, &local);
#endif
  std::ostringstream result;
  result << std::put_time(&local, "%Y-%m-%d");
  return result.str();
}

}  // namespace estimated_taxes
