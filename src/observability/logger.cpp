#include "observability/logger.hpp"

#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>
#include <chrono>

namespace app::observability {
namespace {

std::string NowUtcMs() {
  using namespace std::chrono;
  const auto now = system_clock::now();
  const auto epoch_ms = duration_cast<milliseconds>(now.time_since_epoch()).count();
  return std::to_string(epoch_ms);
}

void Write(const std::string &level,
           const std::string &event,
           const std::unordered_map<std::string, std::string> &fields) {
  nlohmann::json payload = {
      {"ts_unix_ms", NowUtcMs()},
      {"level", level},
      {"event", event},
      {"service", "cpp-microservice-template"},
  };

  for (const auto &[key, value] : fields) {
    payload[key] = value;
  }

  spdlog::info("{}", payload.dump());
}

}  // namespace

void Logger::Init() { spdlog::set_pattern("%v"); }

void Logger::Info(const std::string &event,
                  const std::unordered_map<std::string, std::string> &fields) {
  Write("info", event, fields);
}

void Logger::Error(const std::string &event,
                   const std::unordered_map<std::string, std::string> &fields) {
  Write("error", event, fields);
}

}  // namespace app::observability
