#pragma once

#include <string>
#include <unordered_map>

namespace app::observability {

class Logger {
 public:
  static void Init();
  static void Info(const std::string &event,
                   const std::unordered_map<std::string, std::string> &fields = {});
  static void Error(const std::string &event,
                    const std::unordered_map<std::string, std::string> &fields = {});
};

}  // namespace app::observability
