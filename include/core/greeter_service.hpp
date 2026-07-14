#pragma once

#include <string>

namespace app::core {

class GreeterService {
 public:
  [[nodiscard]] std::string BuildGreeting(const std::string &name) const;
};

}  // namespace app::core
