#include <atomic>
#include <csignal>
#include <cstdlib>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>

#include <grpcpp/ext/proto_server_reflection_plugin.h>
#include <grpcpp/grpcpp.h>
#include <grpcpp/health_check_service_interface.h>

#include "hello_service.hpp"
#include "logger.hpp"
#include "metrics.hpp"
#include "tracer.hpp"

// ---------------------------------------------------------------------------
// Signal-handler state
// Signal handlers may only call async-signal-safe functions.  We store raw
// pointers behind atomics so the handler can issue Shutdown() calls without
// holding any lock (§6.4).
// ---------------------------------------------------------------------------
namespace {

std::atomic<grpc::Server*>   g_server{nullptr};
std::atomic<hello::Metrics*> g_metrics{nullptr};

extern "C" void ShutdownSignalHandler(int /*sig*/) noexcept {
    if (grpc::Server* s = g_server.load(std::memory_order_relaxed)) {
        s->Shutdown();
    }
    if (hello::Metrics* m = g_metrics.load(std::memory_order_relaxed)) {
        m->Shutdown();
    }
}

// Read an environment variable, returning default_val when absent.
int EnvInt(const char* name, int default_val) {
    const char* v = std::getenv(name);
    return v ? std::stoi(v) : default_val;
}

std::string EnvStr(const char* name, std::string_view default_val) {
    const char* v = std::getenv(name);
    return v ? std::string(v) : std::string(default_val);
}

}  // namespace

int main() {
    // ------------------------------------------------------------------
    // 1. Read configuration from environment
    // ------------------------------------------------------------------
    const int         grpc_port    = EnvInt("GRPC_PORT",    50051);
    const uint16_t    metrics_port = static_cast<uint16_t>(
                                         EnvInt("METRICS_PORT", 9090));
    const std::string otlp_endpoint = EnvStr("OTEL_EXPORTER_OTLP_ENDPOINT",
                                             "localhost:4317");

    // ------------------------------------------------------------------
    // 2. Construct observability stack
    //    Destruction order (LIFO) guaranteed by declaration order (§6.4):
    //    tracer → metrics → logger
    //    gRPC server is shut down first (via signal handler) before these
    //    destructors run.
    // ------------------------------------------------------------------
    hello::Logger  logger ("hello", hello::Logger::LevelFromEnv());
    hello::Metrics metrics("hello", metrics_port);
    hello::Tracer  tracer ("hello", otlp_endpoint);

    logger.Info("Starting hello_server on port " + std::to_string(grpc_port));

    // ------------------------------------------------------------------
    // 3. Build gRPC server
    // ------------------------------------------------------------------
    hello::HelloServiceImpl service(logger, metrics, tracer);

    grpc::EnableDefaultHealthCheckService(true);
    grpc::reflection::InitProtoReflectionServerBuilderPlugin();

    grpc::ServerBuilder builder;
    builder.AddListeningPort("0.0.0.0:" + std::to_string(grpc_port),
                             grpc::InsecureServerCredentials());
    builder.RegisterService(&service);

    std::unique_ptr<grpc::Server> server = builder.BuildAndStart();
    if (!server) {
        logger.Error("Failed to start gRPC server — aborting");
        return 1;
    }

    // ------------------------------------------------------------------
    // 4. Install POSIX signal handlers AFTER server is built
    // ------------------------------------------------------------------
    g_server.store(server.get(),  std::memory_order_relaxed);
    g_metrics.store(&metrics,     std::memory_order_relaxed);

    std::signal(SIGINT,  ShutdownSignalHandler);
    std::signal(SIGTERM, ShutdownSignalHandler);

    logger.Info("gRPC server listening on port " + std::to_string(grpc_port));
    logger.Info("Metrics HTTP server on port " + std::to_string(metrics_port));

    // ------------------------------------------------------------------
    // 5. Start metrics HTTP server on a dedicated thread (§5.3)
    // ------------------------------------------------------------------
    std::thread metrics_thread([&metrics]() {
        metrics.ServeForever();
    });

    // ------------------------------------------------------------------
    // 6. Block until Shutdown() is called by signal handler
    // ------------------------------------------------------------------
    server->Wait();

    // Clear signal-handler pointers before objects go out of scope.
    g_server.store(nullptr,  std::memory_order_relaxed);
    g_metrics.store(nullptr, std::memory_order_relaxed);

    // ------------------------------------------------------------------
    // 7. Graceful teardown (§6.4)
    // ------------------------------------------------------------------
    metrics_thread.join();   // HTTP thread already stopped via Shutdown()

    logger.Info("Server stopped");
    return 0;
}
