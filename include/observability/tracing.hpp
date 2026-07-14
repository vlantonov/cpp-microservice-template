#pragma once

#include <string>

#include <opentelemetry/nostd/shared_ptr.h>

namespace opentelemetry::trace {
class Tracer;
}

namespace app::observability {

class Tracing {
 public:
  static void Init(const std::string &service_name,
                   const std::string &otlp_http_endpoint);
  static opentelemetry::nostd::shared_ptr<opentelemetry::trace::Tracer> GetTracer(
      const std::string &library_name);
};

}  // namespace app::observability
