#include "transport/rest/rest_server.hpp"

#include "core/greeter_service.hpp"
#include "observability/logger.hpp"
#include "observability/metrics.hpp"
#include "observability/tracing.hpp"

#include <httplib.h>
#include <nlohmann/json.hpp>
#include <opentelemetry/trace/scope.h>

namespace app::transport::rest {

RestServer::RestServer(std::shared_ptr<app::core::GreeterService> service,
                       std::shared_ptr<app::observability::Metrics> metrics,
                       std::string host, int port)
    : service_(std::move(service)),
      metrics_(std::move(metrics)),
      host_(std::move(host)),
      port_(port) {}

void RestServer::RunBlocking() {
  httplib::Server http;

  http.Get("/healthz", [](const httplib::Request &, httplib::Response &res) {
    res.status = 200;
    res.set_content("ok", "text/plain");
  });

  http.Post("/api/v1/hello",
            [this](const httplib::Request &req, httplib::Response &res) {
              auto tracer = app::observability::Tracing::GetTracer("transport.rest");
              auto span = tracer->StartSpan("rest.POST./api/v1/hello");
              opentelemetry::trace::Scope scope(span);

              try {
                const auto json_req = nlohmann::json::parse(req.body);
                const auto name = json_req.value("name", "");
                const auto greeting = service_->BuildGreeting(name);

                nlohmann::json json_res = {{"message", greeting}};
                res.status = 200;
                res.set_content(json_res.dump(), "application/json");

                metrics_->ObserveRequest("rest", "/api/v1/hello", "ok", 0.001);
                app::observability::Logger::Info(
                    "rest_request",
                    {{"route", "/api/v1/hello"}, {"status", "ok"}});
              } catch (const std::exception &ex) {
                nlohmann::json err = {{"error", "invalid request"}};
                res.status = 400;
                res.set_content(err.dump(), "application/json");

                metrics_->ObserveRequest("rest", "/api/v1/hello", "error", 0.001);
                app::observability::Logger::Error(
                    "rest_request_failed",
                    {{"route", "/api/v1/hello"}, {"error", ex.what()}});
              }

              span->End();
            });

  app::observability::Logger::Info("rest_server_started",
                                   {{"host", host_}, {"port", std::to_string(port_)}});
  http.listen(host_, port_);
}

}  // namespace app::transport::rest
