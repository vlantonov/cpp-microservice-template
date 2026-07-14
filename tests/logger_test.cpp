#include <gtest/gtest.h>

#include <cstdio>
#include <cstdlib>
#include <string>

#include <spdlog/spdlog.h>

#include "logger.hpp"

namespace {

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------

TEST(LoggerTest, ConstructsWithoutThrowing) {
    EXPECT_NO_THROW(hello::Logger("test-svc", spdlog::level::debug));
}

// ---------------------------------------------------------------------------
// LevelFromEnv
// ---------------------------------------------------------------------------

// Clean up after each LevelFromEnv test.
class LevelFromEnvTestFixture : public ::testing::Test {
protected:
    void TearDown() override { unsetenv("LOG_LEVEL"); }
};

TEST_F(LevelFromEnvTestFixture, AbsentEnvReturnsInfo) {
    unsetenv("LOG_LEVEL");
    EXPECT_EQ(hello::Logger::LevelFromEnv(), spdlog::level::info);
}

TEST_F(LevelFromEnvTestFixture, DebugLowercase) {
    setenv("LOG_LEVEL", "debug", 1);
    EXPECT_EQ(hello::Logger::LevelFromEnv(), spdlog::level::debug);
}

TEST_F(LevelFromEnvTestFixture, DebugUppercase) {
    setenv("LOG_LEVEL", "DEBUG", 1);
    EXPECT_EQ(hello::Logger::LevelFromEnv(), spdlog::level::debug);
}

TEST_F(LevelFromEnvTestFixture, WarnLowercase) {
    setenv("LOG_LEVEL", "warn", 1);
    EXPECT_EQ(hello::Logger::LevelFromEnv(), spdlog::level::warn);
}

TEST_F(LevelFromEnvTestFixture, WarnWarning) {
    setenv("LOG_LEVEL", "warning", 1);
    EXPECT_EQ(hello::Logger::LevelFromEnv(), spdlog::level::warn);
}

TEST_F(LevelFromEnvTestFixture, ErrorUppercase) {
    setenv("LOG_LEVEL", "ERROR", 1);
    EXPECT_EQ(hello::Logger::LevelFromEnv(), spdlog::level::err);
}

TEST_F(LevelFromEnvTestFixture, UnrecognisedValueReturnsInfo) {
    setenv("LOG_LEVEL", "verbose", 1);
    EXPECT_EQ(hello::Logger::LevelFromEnv(), spdlog::level::info);
}

// ---------------------------------------------------------------------------
// Log methods don't throw for normal input
// ---------------------------------------------------------------------------

class LogMethodsTest : public ::testing::Test {
protected:
    hello::Logger logger_{"test", spdlog::level::debug};
};

TEST_F(LogMethodsTest, DebugDoesNotThrow) {
    EXPECT_NO_THROW(logger_.Debug("debug message"));
}

TEST_F(LogMethodsTest, InfoDoesNotThrow) {
    EXPECT_NO_THROW(logger_.Info("info message"));
}

TEST_F(LogMethodsTest, WarnDoesNotThrow) {
    EXPECT_NO_THROW(logger_.Warn("warn message"));
}

TEST_F(LogMethodsTest, ErrorDoesNotThrow) {
    EXPECT_NO_THROW(logger_.Error("error message"));
}

TEST_F(LogMethodsTest, InfoWithTraceId) {
    EXPECT_NO_THROW(
        logger_.Info("with trace", "4bf92f3577b34da6a3ce929d0e0e4736"));
}

TEST_F(LogMethodsTest, MessageWithSpecialJsonChars) {
    // Double-quotes, backslashes, and newlines must not crash.
    EXPECT_NO_THROW(logger_.Info("message with \"quotes\" and \\slashes\n"));
}

// ---------------------------------------------------------------------------
// FR-9: Verify JSON output contains all required fields
// ---------------------------------------------------------------------------

TEST_F(LogMethodsTest, InfoOutputsJsonWithAllRequiredFields) {
    testing::internal::CaptureStdout();
    logger_.Info("test message", "4bf92f3577b34da6a3ce929d0e0e4736");
    std::fflush(stdout);
    const std::string output = testing::internal::GetCapturedStdout();

    EXPECT_NE(output.find("\"timestamp\""), std::string::npos)
        << "Missing 'timestamp' field in JSON log output";
    EXPECT_NE(output.find("\"severity\":\"INFO\""), std::string::npos)
        << "Missing or wrong 'severity' field in JSON log output";
    EXPECT_NE(output.find("\"service\":\"test\""), std::string::npos)
        << "Missing 'service' field in JSON log output";
    EXPECT_NE(output.find("\"trace_id\":\"4bf92f3577b34da6a3ce929d0e0e4736\""),
              std::string::npos)
        << "Missing 'trace_id' field in JSON log output";
    EXPECT_NE(output.find("\"message\":\"test message\""), std::string::npos)
        << "Missing 'message' field in JSON log output";
    // Output must be a single JSON object per line.
    const auto newline = output.find('\n');
    ASSERT_NE(newline, std::string::npos) << "Log record has no newline terminator";
    const std::string record = output.substr(0, newline);
    EXPECT_EQ(record.front(), '{') << "Log record does not start with '{'";
    EXPECT_EQ(record.back(),  '}') << "Log record does not end with '}'";
}

TEST_F(LogMethodsTest, WarnOutputsCorrectSeverityField) {
    testing::internal::CaptureStdout();
    logger_.Warn("warn test");
    std::fflush(stdout);
    const std::string output = testing::internal::GetCapturedStdout();
    EXPECT_NE(output.find("\"severity\":\"WARN\""), std::string::npos)
        << "WARN severity field not found in log output";
}

TEST_F(LogMethodsTest, ErrorOutputsCorrectSeverityField) {
    testing::internal::CaptureStdout();
    logger_.Error("error test");
    std::fflush(stdout);
    const std::string output = testing::internal::GetCapturedStdout();
    EXPECT_NE(output.find("\"severity\":\"ERROR\""), std::string::npos)
        << "ERROR severity field not found in log output";
}

}  // namespace
