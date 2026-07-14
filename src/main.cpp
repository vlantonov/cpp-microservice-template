#include "core/greeter_service.hpp"
#include "observability/logger.hpp"
#include "observability/metrics.hpp"
#include "observability/tracing.hpp"
#include "transport/grpc/greeter_server.hpp"
#include "transport/rest/rest_server.hpp"

#include <memory>
#include <thread>

int main() {
  app::observability::Logger::Init();
  app::observability::Tracing::Init("cpp-microservice-template",
                                    "http://127.0.0.1:4318/v1/traces");

  auto metrics = std::make_shared<app::observability::Metrics>();
  metrics->StartEndpoint("0.0.0.0:9464");

  auto service = std::make_shared<app::core::GreeterService>();

  app::transport::rest::RestServer rest_server(service, metrics, "0.0.0.0", 8080);
  app::transport::grpc::GreeterGrpcServer grpc_server(service, metrics,
                                                      "0.0.0.0:50051");

  std::thread grpc_thread([&grpc_server]() { grpc_server.RunBlocking(); });
  rest_server.RunBlocking();

  grpc_thread.join();
  return 0;
}
