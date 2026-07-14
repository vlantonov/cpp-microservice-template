#include "logger.hpp"

#include <chrono>
#include <cstdlib>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <string>

#include <spdlog/sinks/stdout_sinks.h>

namespace hello {

namespace {

// Returns current UTC time formatted as ISO-8601 with millisecond precision.
std::string CurrentTimestamp() {
    const auto now   = std::chrono::system_clock::now();
    const auto ms    = std::chrono::duration_cast<std::chrono::milliseconds>(
                           now.time_since_epoch()) % 1000;
    const std::time_t t = std::chrono::system_clock::to_time_t(now);
    std::tm bt{};
    gmtime_r(&t, &bt);

    std::ostringstream oss;
    oss << std::put_time(&bt, "%Y-%m-%dT%H:%M:%S");
    oss << '.' << std::setfill('0') << std::setw(3) << ms.count() << 'Z';
    return oss.str();
}

// Escapes characters that would break a JSON string value.
// Only handles the common cases needed for log messages.
std::string JsonEscape(std::string_view s) {
    std::string out;
    out.reserve(s.size());
    for (const char c : s) {
        switch (c) {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n";  break;
            case '\r': out += "\\r";  break;
            case '\t': out += "\\t";  break;
            default:   out += c;      break;
        }
    }
    return out;
}

// Builds the JSON log record as a single line.
std::string MakeJson(std::string_view severity,
                     std::string_view service,
                     std::string_view trace_id,
                     std::string_view message) {
    return std::string("{")
        + "\"timestamp\":\"" + CurrentTimestamp()        + "\","
        + "\"severity\":\""  + std::string(severity)     + "\","
        + "\"service\":\""   + std::string(service)      + "\","
        + "\"trace_id\":\""  + std::string(trace_id)     + "\","
        + "\"message\":\""   + JsonEscape(message)        + "\""
        + "}";
}

}  // namespace

Logger::Logger(std::string_view service_name, spdlog::level::level_enum level)
    : service_name_(service_name) {
    auto sink = std::make_shared<spdlog::sinks::stdout_sink_mt>();
    impl_ = std::make_shared<spdlog::logger>(std::string(service_name), sink);
    impl_->set_level(level);
    // Use bare message pattern — JSON is constructed manually.
    impl_->set_pattern("%v");
}

void Logger::Debug(std::string_view message, std::string_view trace_id) {
    impl_->debug(MakeJson("DEBUG", service_name_, trace_id, message));
}

void Logger::Info(std::string_view message, std::string_view trace_id) {
    impl_->info(MakeJson("INFO", service_name_, trace_id, message));
}

void Logger::Warn(std::string_view message, std::string_view trace_id) {
    impl_->warn(MakeJson("WARN", service_name_, trace_id, message));
}

void Logger::Error(std::string_view message, std::string_view trace_id) {
    impl_->error(MakeJson("ERROR", service_name_, trace_id, message));
}

spdlog::level::level_enum Logger::LevelFromEnv() {
    const char* env = std::getenv("LOG_LEVEL");
    if (env == nullptr) { return spdlog::level::info; }

    const std::string val(env);
    if (val == "debug"   || val == "DEBUG")   { return spdlog::level::debug; }
    if (val == "info"    || val == "INFO")    { return spdlog::level::info;  }
    if (val == "warn"    || val == "WARN"
     || val == "warning" || val == "WARNING") { return spdlog::level::warn;  }
    if (val == "error"   || val == "ERROR")   { return spdlog::level::err;   }

    return spdlog::level::info;
}

}  // namespace hello
