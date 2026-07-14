#include "core/greeter_service.hpp"

namespace app::core {

std::string GreeterService::BuildGreeting(const std::string &name) const {
  if (name.empty()) {
    return "Hello, anonymous client";
  }
  return "Hello, " + name;
}

}  // namespace app::core
