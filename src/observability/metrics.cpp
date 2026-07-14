#include "observability/metrics.hpp"

#include <prometheus/counter.h>
#include <prometheus/exposer.h>
#include <prometheus/family.h>
#include <prometheus/histogram.h>
#include <prometheus/registry.h>

namespace app::observability {

Metrics::LabelKey Metrics::MakeKey(const std::string &transport,
                                   const std::string &route,
                                   const std::string &status) {
  return transport + "|" + route + "|" + status;
}

Metrics::Metrics() : registry_(std::make_shared<prometheus::Registry>()) {
  requests_total_family_ = &prometheus::BuildCounter()
                                .Name("app_requests_total")
                                .Help("Total requests handled by the service")
                                .Register(*registry_);

  request_duration_family_ = &prometheus::BuildHistogram()
                                  .Name("app_request_duration_seconds")
                                  .Help("Request duration in seconds")
                                  .Register(*registry_);
}

Metrics::~Metrics() = default;

void Metrics::StartEndpoint(const std::string &bind_address) {
  exposer_ = std::make_unique<prometheus::Exposer>(bind_address);
  exposer_->RegisterCollectable(registry_);
}

void Metrics::ObserveRequest(const std::string &transport, const std::string &route,
                             const std::string &status, double duration_seconds) {
  const auto key = MakeKey(transport, route, status);
  std::lock_guard<std::mutex> lock(mutex_);

  if (!counters_.contains(key)) {
    counters_[key] = &requests_total_family_->Add(
        {{"transport", transport}, {"route", route}, {"status", status}});
  }

  if (!histograms_.contains(key)) {
    histograms_[key] = &request_duration_family_->Add(
        {{"transport", transport}, {"route", route}, {"status", status}},
        {0.001, 0.005, 0.01, 0.05, 0.1, 0.25, 0.5, 1.0, 2.0});
  }

  counters_[key]->Increment();
  histograms_[key]->Observe(duration_seconds);
}

}  // namespace app::observability
