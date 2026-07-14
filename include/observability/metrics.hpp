#pragma once

#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>

namespace prometheus {
class Exposer;
class Registry;
class Counter;
class Histogram;
class FamilyBase;
template <typename T>
class Family;
}  // namespace prometheus

namespace app::observability {

class Metrics {
 public:
  Metrics();
  ~Metrics();

  void StartEndpoint(const std::string &bind_address);
  void ObserveRequest(const std::string &transport, const std::string &route,
                      const std::string &status, double duration_seconds);

 private:
  using LabelKey = std::string;

  [[nodiscard]] static LabelKey MakeKey(const std::string &transport,
                                        const std::string &route,
                                        const std::string &status);

  std::unique_ptr<prometheus::Exposer> exposer_;
  std::shared_ptr<prometheus::Registry> registry_;
  prometheus::Family<prometheus::Counter> *requests_total_family_{nullptr};
  prometheus::Family<prometheus::Histogram> *request_duration_family_{nullptr};
  std::unordered_map<LabelKey, prometheus::Counter *> counters_;
  std::unordered_map<LabelKey, prometheus::Histogram *> histograms_;
  std::mutex mutex_;
};

}  // namespace app::observability
