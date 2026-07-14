#pragma once

#include <cstdint>
#include <memory>
#include <string_view>

namespace hello {

/**
 * @brief Prometheus metrics registry with an embedded HTTP exposition server.
 *
 * Uses PIMPL to keep prometheus-cpp headers out of every consumer TU.
 * All public metric-mutation methods are thread-safe (prometheus-cpp uses
 * atomic operations internally).
 *
 * Metric families registered:
 *  - rpc_requests_total        Counter   {method, status, service}
 *  - rpc_request_duration_seconds Histogram {method, service}
 *  - rpc_active_connections    Gauge     {service}
 */
class Metrics {
public:
    /**
     * @brief Constructs the Metrics registry and configures the HTTP handler.
     * @param service_name  Label value used for the "service" label on all metrics.
     * @param http_port     TCP port for the /metrics HTTP endpoint.
     */
    Metrics(std::string_view service_name, uint16_t http_port);

    /** @brief Destructor — stops HTTP server if still running. */
    ~Metrics();

    // Non-copyable, non-movable (owns OS socket and registry pointers).
    Metrics(const Metrics&)            = delete;
    Metrics& operator=(const Metrics&) = delete;
    Metrics(Metrics&&)                 = delete;
    Metrics& operator=(Metrics&&)      = delete;

    /**
     * @brief Increments rpc_requests_total{method, status, service}.
     * @param method  RPC method name, e.g. "SayHello".
     * @param status  gRPC status code string, e.g. "OK" or "INVALID_ARGUMENT".
     */
    void RecordRequest(std::string_view method, std::string_view status);

    /**
     * @brief Records a latency sample in rpc_request_duration_seconds.
     * @param method           RPC method name.
     * @param duration_seconds Observed latency in seconds.
     */
    void ObserveRequestDuration(std::string_view method, double duration_seconds);

    /**
     * @brief Sets rpc_active_connections{service} to the given value.
     * @param count Current number of active gRPC connections.
     */
    void SetActiveConnections(double count);

    /**
     * @brief Starts the cpp-httplib server.  Blocks until Shutdown() is called.
     *
     * Must be invoked on a dedicated std::thread.
     */
    void ServeForever();

    /**
     * @brief Signals the HTTP server to stop; ServeForever() returns promptly.
     */
    void Shutdown();

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace hello
