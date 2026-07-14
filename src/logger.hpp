#pragma once

#include <memory>
#include <string>
#include <string_view>

#include <spdlog/spdlog.h>

namespace hello {

/**
 * @brief Structured JSON logger backed by spdlog.
 *
 * Every log record is emitted as a single-line JSON object to stdout:
 * @code
 * {"timestamp":"2026-07-14T12:00:00.000Z","severity":"INFO",
 *  "service":"hello","trace_id":"4bf92f3577b34da6a3ce929d0e0e4736",
 *  "message":"RPC SayHello completed"}
 * @endcode
 *
 * Thread-safety: all public methods are thread-safe via spdlog's internal mutex.
 */
class Logger {
public:
    /**
     * @brief Constructs a Logger.
     * @param service_name Value written to the "service" JSON field on every record.
     * @param level        Initial severity filter; records below this level are dropped.
     */
    Logger(std::string_view service_name, spdlog::level::level_enum level);

    /** @brief Emit a DEBUG-level JSON log record. */
    void Debug(std::string_view message, std::string_view trace_id = "");

    /** @brief Emit an INFO-level JSON log record. */
    void Info(std::string_view message, std::string_view trace_id = "");

    /** @brief Emit a WARN-level JSON log record. */
    void Warn(std::string_view message, std::string_view trace_id = "");

    /** @brief Emit an ERROR-level JSON log record. */
    void Error(std::string_view message, std::string_view trace_id = "");

    /**
     * @brief Parse the LOG_LEVEL environment variable.
     * @return The parsed level, or spdlog::level::info if absent or unrecognised.
     */
    static spdlog::level::level_enum LevelFromEnv();

private:
    std::shared_ptr<spdlog::logger> impl_;
    std::string service_name_;
};

}  // namespace hello
