#include "metrics.hpp"

#include <memory>
#include <string>
#include <vector>

// prometheus-cpp — only included in this TU (PIMPL)
#include <prometheus/counter.h>
#include <prometheus/gauge.h>
#include <prometheus/histogram.h>
#include <prometheus/registry.h>
#include <prometheus/text_serializer.h>

// cpp-httplib — only included in this TU
#include <httplib.h>

namespace hello {

namespace {

// Default latency histogram bucket boundaries (seconds).
const prometheus::Histogram::BucketBoundaries kDurationBuckets = {
    0.001, 0.005, 0.01, 0.025, 0.05, 0.1, 0.25, 0.5, 1.0, 2.5, 5.0, 10.0
};

}  // namespace

// ---------------------------------------------------------------------------
// PIMPL implementation struct
// ---------------------------------------------------------------------------
struct Metrics::Impl {
    std::string  service_name;
    uint16_t     http_port;

    std::shared_ptr<prometheus::Registry> registry;

    // Metric families — lazily populated with label combinations on first use.
    prometheus::Family<prometheus::Counter>&   requests_total;
    prometheus::Family<prometheus::Histogram>& request_duration;
    prometheus::Family<prometheus::Gauge>&     active_connections;

    httplib::Server http_server;

    explicit Impl(std::string_view svc_name, uint16_t port)
        : service_name(svc_name)
        , http_port(port)
        , registry(std::make_shared<prometheus::Registry>())
        , requests_total(
              prometheus::BuildCounter()
                  .Name("rpc_requests_total")
                  .Help("Total RPC calls received")
                  .Register(*registry))
        , request_duration(
              prometheus::BuildHistogram()
                  .Name("rpc_request_duration_seconds")
                  .Help("RPC handler latency")
                  .Register(*registry))
        , active_connections(
              prometheus::BuildGauge()
                  .Name("rpc_active_connections")
                  .Help("Currently active gRPC connections")
                  .Register(*registry)) {}
};

// ---------------------------------------------------------------------------
// Metrics public API
// ---------------------------------------------------------------------------

Metrics::Metrics(std::string_view service_name, uint16_t http_port)
    : impl_(std::make_unique<Impl>(service_name, http_port)) {

    // Pre-initialise the active-connections gauge so it always appears in the
    // /metrics output (prometheus-cpp only serialises families that have at
    // least one child instance).
    impl_->active_connections
        .Add({{"service", impl_->service_name}})
        .Set(0.0);

    // Register the /metrics HTTP handler.  Capture raw pointer — the lambda
    // lifetime is bounded by impl_->http_server which lives inside impl_.
    Impl* impl_raw = impl_.get();

    impl_->http_server.Get("/metrics",
        [impl_raw](const httplib::Request& /*req*/, httplib::Response& res) {
            prometheus::TextSerializer serializer;
            const auto collected = impl_raw->registry->Collect();
            res.set_content(serializer.Serialize(collected),
                            "text/plain; version=0.0.4; charset=utf-8");
        });
}

Metrics::~Metrics() {
    if (impl_) {
        impl_->http_server.stop();
    }
}

void Metrics::RecordRequest(std::string_view method, std::string_view status) {
    impl_->requests_total
        .Add({{"method",  std::string(method)},
              {"status",  std::string(status)},
              {"service", impl_->service_name}})
        .Increment();
}

void Metrics::ObserveRequestDuration(std::string_view method, double duration_seconds) {
    impl_->request_duration
        .Add({{"method",  std::string(method)},
              {"service", impl_->service_name}},
             kDurationBuckets)
        .Observe(duration_seconds);
}

void Metrics::SetActiveConnections(double count) {
    impl_->active_connections
        .Add({{"service", impl_->service_name}})
        .Set(count);
}

void Metrics::ServeForever() {
    impl_->http_server.listen("0.0.0.0", impl_->http_port);
}

void Metrics::Shutdown() {
    impl_->http_server.stop();
}

}  // namespace hello
