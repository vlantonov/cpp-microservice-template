#include <gtest/gtest.h>

#include <chrono>
#include <string>
#include <thread>

#include <httplib.h>

#include "metrics.hpp"

namespace {

// Use a dedicated test port to avoid conflicts with production port 9090.
constexpr uint16_t kTestPort = 19090;

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------

TEST(MetricsTest, ConstructsWithoutThrowing) {
    EXPECT_NO_THROW(hello::Metrics("test", kTestPort));
}

// ---------------------------------------------------------------------------
// Metric mutation methods
// ---------------------------------------------------------------------------

class MetricsMutationTest : public ::testing::Test {
protected:
    hello::Metrics metrics_{"test", kTestPort};
};

TEST_F(MetricsMutationTest, RecordRequestDoesNotThrow) {
    EXPECT_NO_THROW(metrics_.RecordRequest("SayHello", "OK"));
}

TEST_F(MetricsMutationTest, RecordRequestMultipleStatuses) {
    EXPECT_NO_THROW(metrics_.RecordRequest("SayHello", "OK"));
    EXPECT_NO_THROW(metrics_.RecordRequest("SayHello", "INVALID_ARGUMENT"));
    EXPECT_NO_THROW(metrics_.RecordRequest("SayHello", "INTERNAL"));
}

TEST_F(MetricsMutationTest, ObserveRequestDurationDoesNotThrow) {
    EXPECT_NO_THROW(metrics_.ObserveRequestDuration("SayHello", 0.001));
    EXPECT_NO_THROW(metrics_.ObserveRequestDuration("SayHello", 1.5));
}

TEST_F(MetricsMutationTest, SetActiveConnectionsDoesNotThrow) {
    EXPECT_NO_THROW(metrics_.SetActiveConnections(0.0));
    EXPECT_NO_THROW(metrics_.SetActiveConnections(42.0));
}

// ---------------------------------------------------------------------------
// HTTP /metrics endpoint
// ---------------------------------------------------------------------------

TEST(MetricsHttpTest, EndpointReturnsPrometheusTextFormat) {
    constexpr uint16_t kHttpPort = 19091;
    hello::Metrics metrics("test_svc", kHttpPort);

    // Record a request so there is at least one non-zero metric.
    metrics.RecordRequest("SayHello", "OK");
    metrics.ObserveRequestDuration("SayHello", 0.05);

    // Start the HTTP server on a background thread.
    std::thread serve_thread([&metrics]() { metrics.ServeForever(); });

    // Give the server a moment to bind and start accepting connections.
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    httplib::Client client("127.0.0.1", kHttpPort);
    client.set_connection_timeout(2);

    const auto result = client.Get("/metrics");
    ASSERT_TRUE(result) << "HTTP GET /metrics failed";
    EXPECT_EQ(result->status, 200);
    EXPECT_NE(result->body.find("rpc_requests_total"), std::string::npos);
    EXPECT_NE(result->body.find("rpc_request_duration_seconds"), std::string::npos);
    EXPECT_NE(result->body.find("rpc_active_connections"), std::string::npos);

    metrics.Shutdown();
    serve_thread.join();
}

TEST(MetricsHttpTest, ShutdownStopsServer) {
    constexpr uint16_t kShutdownPort = 19092;
    hello::Metrics metrics("test_shutdown", kShutdownPort);

    std::thread serve_thread([&metrics]() { metrics.ServeForever(); });
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    EXPECT_NO_THROW(metrics.Shutdown());
    EXPECT_NO_THROW(serve_thread.join());
}

}  // namespace
