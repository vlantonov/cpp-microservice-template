#include <gtest/gtest.h>

#include <chrono>
#include <memory>
#include <string>
#include <thread>

#include <grpcpp/grpcpp.h>
#include <httplib.h>
#include <spdlog/spdlog.h>

#include "hello_service.hpp"
#include "logger.hpp"
#include "metrics.hpp"
#include "tracer.hpp"
#include "hello/v1/hello.grpc.pb.h"

namespace {

constexpr std::string_view kTestOtlpEndpoint = "localhost:19317";

// ---------------------------------------------------------------------------
// Test fixture — owns the observability stack + service under test
// ---------------------------------------------------------------------------

class HelloServiceTest : public ::testing::Test {
protected:
    void SetUp() override {
        logger_  = std::make_unique<hello::Logger>("test", spdlog::level::debug);
        metrics_ = std::make_unique<hello::Metrics>("test", metrics_port_);
        tracer_  = std::make_unique<hello::Tracer>("test", kTestOtlpEndpoint);
        service_ = std::make_unique<hello::HelloServiceImpl>(
            *logger_, *metrics_, *tracer_);
    }

    void TearDown() override {
        service_.reset();
        tracer_.reset();
        metrics_.reset();
        logger_.reset();
    }

    // Returns a default grpc::ServerContext suitable for unit testing.
    grpc::ServerContext MakeCtx() { return {}; }

    // Unique metrics port per test suite run to avoid TOCTOU with other tests.
    static constexpr uint16_t metrics_port_ = 19093;

    std::unique_ptr<hello::Logger>           logger_;
    std::unique_ptr<hello::Metrics>          metrics_;
    std::unique_ptr<hello::Tracer>           tracer_;
    std::unique_ptr<hello::HelloServiceImpl> service_;
};

// ---------------------------------------------------------------------------
// Happy path: echoes the input wrapped in a greeting
// ---------------------------------------------------------------------------

TEST_F(HelloServiceTest, SayHelloHappyPath) {
    grpc::ServerContext ctx;
    hello::v1::HelloRequest  req;
    hello::v1::HelloResponse resp;

    req.set_message("World");
    const grpc::Status status = service_->SayHello(&ctx, &req, &resp);

    EXPECT_TRUE(status.ok());
    EXPECT_EQ(resp.message(), "Hello, World!");
}

TEST_F(HelloServiceTest, SayHelloReturnsInputInResponse) {
    grpc::ServerContext ctx;
    hello::v1::HelloRequest  req;
    hello::v1::HelloResponse resp;

    req.set_message("gRPC");
    const grpc::Status status = service_->SayHello(&ctx, &req, &resp);

    EXPECT_TRUE(status.ok());
    EXPECT_NE(resp.message().find("gRPC"), std::string::npos);
}

// ---------------------------------------------------------------------------
// Error path: empty message → INVALID_ARGUMENT
// ---------------------------------------------------------------------------

TEST_F(HelloServiceTest, SayHelloEmptyMessageReturnsInvalidArgument) {
    grpc::ServerContext ctx;
    hello::v1::HelloRequest  req;
    hello::v1::HelloResponse resp;

    // message field left empty (default proto value).
    const grpc::Status status = service_->SayHello(&ctx, &req, &resp);

    EXPECT_FALSE(status.ok());
    EXPECT_EQ(status.error_code(), grpc::StatusCode::INVALID_ARGUMENT);
    EXPECT_FALSE(status.error_message().empty());
}

TEST_F(HelloServiceTest, SayHelloEmptyStringExplicitlySet) {
    grpc::ServerContext ctx;
    hello::v1::HelloRequest  req;
    hello::v1::HelloResponse resp;

    req.set_message("");
    const grpc::Status status = service_->SayHello(&ctx, &req, &resp);

    EXPECT_EQ(status.error_code(), grpc::StatusCode::INVALID_ARGUMENT);
}

// ---------------------------------------------------------------------------
// Metrics side-effect: counter incremented after calls
// ---------------------------------------------------------------------------

TEST(HelloServiceMetricsTest, MetricsIncrementedAfterSuccessfulCall) {
    constexpr uint16_t kPort = 19094;

    hello::Logger  logger ("test_metrics", spdlog::level::warn);
    hello::Metrics metrics("test_metrics", kPort);
    hello::Tracer  tracer ("test_metrics", kTestOtlpEndpoint);
    hello::HelloServiceImpl service(logger, metrics, tracer);

    // Start metrics HTTP server.
    std::thread serve_thread([&metrics]() { metrics.ServeForever(); });
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    grpc::ServerContext ctx;
    hello::v1::HelloRequest  req;
    hello::v1::HelloResponse resp;
    req.set_message("metrics-test");
    service.SayHello(&ctx, &req, &resp);

    // Query the Prometheus text endpoint and verify the counter exists.
    httplib::Client client("127.0.0.1", kPort);
    client.set_connection_timeout(2);

    const auto result = client.Get("/metrics");
    ASSERT_TRUE(result) << "Failed to reach /metrics endpoint";
    EXPECT_EQ(result->status, 200);
    EXPECT_NE(result->body.find("rpc_requests_total"), std::string::npos)
        << "rpc_requests_total not found in metrics output";
    // The OK label should be present after a successful call.
    EXPECT_NE(result->body.find("\"OK\""), std::string::npos)
        << "OK status label not found in rpc_requests_total counter";

    metrics.Shutdown();
    serve_thread.join();
}

TEST(HelloServiceMetricsTest, MetricsRecordErrorStatusOnInvalidArgument) {
    constexpr uint16_t kPort = 19095;

    hello::Logger  logger ("test_error_metrics", spdlog::level::warn);
    hello::Metrics metrics("test_error_metrics", kPort);
    hello::Tracer  tracer ("test_error_metrics", kTestOtlpEndpoint);
    hello::HelloServiceImpl service(logger, metrics, tracer);

    std::thread serve_thread([&metrics]() { metrics.ServeForever(); });
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Empty message → INVALID_ARGUMENT
    grpc::ServerContext ctx;
    hello::v1::HelloRequest  req;
    hello::v1::HelloResponse resp;
    service.SayHello(&ctx, &req, &resp);

    httplib::Client client("127.0.0.1", kPort);
    client.set_connection_timeout(2);

    const auto result = client.Get("/metrics");
    ASSERT_TRUE(result);
    EXPECT_EQ(result->status, 200);
    EXPECT_NE(result->body.find("INVALID_ARGUMENT"), std::string::npos)
        << "INVALID_ARGUMENT status not found after error call";

    metrics.Shutdown();
    serve_thread.join();
}

}  // namespace
