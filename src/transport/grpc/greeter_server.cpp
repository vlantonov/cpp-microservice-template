#include "transport/grpc/greeter_server.hpp"

#include "core/greeter_service.hpp"
#include "observability/logger.hpp"
#include "observability/metrics.hpp"
#include "observability/tracing.hpp"

#include <grpcpp/grpcpp.h>
#include <opentelemetry/trace/scope.h>

#include "greeter/v1/greeter.grpc.pb.h"

namespace app::transport::grpc {
namespace {

class GreeterServiceImpl final : public greeter::v1::Greeter::Service {
 public:
  GreeterServiceImpl(std::shared_ptr<app::core::GreeterService> service,
                     std::shared_ptr<app::observability::Metrics> metrics)
      : service_(std::move(service)), metrics_(std::move(metrics)) {}

  grpc::Status SayHello(grpc::ServerContext *context,
                        const greeter::v1::HelloRequest *request,
                        greeter::v1::HelloReply *reply) override {
    (void)context;
    auto tracer = app::observability::Tracing::GetTracer("transport.grpc");
    auto span = tracer->StartSpan("grpc.SayHello");
    opentelemetry::trace::Scope scope(span);

    const auto message = service_->BuildGreeting(request->name());
    reply->set_message(message);

    metrics_->ObserveRequest("grpc", "/greeter.v1.Greeter/SayHello", "ok", 0.001);
    app::observability::Logger::Info("grpc_request", {{"method", "SayHello"}, {"status", "ok"}});

    span->End();
    return grpc::Status::OK;
  }

 private:
  std::shared_ptr<app::core::GreeterService> service_;
  std::shared_ptr<app::observability::Metrics> metrics_;
};

}  // namespace

GreeterGrpcServer::GreeterGrpcServer(
    std::shared_ptr<app::core::GreeterService> service,
    std::shared_ptr<app::observability::Metrics> metrics, std::string listen_address)
    : service_(std::move(service)),
      metrics_(std::move(metrics)),
      listen_address_(std::move(listen_address)) {}

void GreeterGrpcServer::RunBlocking() {
  GreeterServiceImpl service_impl(service_, metrics_);

  grpc::ServerBuilder builder;
  builder.AddListeningPort(listen_address_, grpc::InsecureServerCredentials());
  builder.RegisterService(&service_impl);

  auto server = builder.BuildAndStart();
  app::observability::Logger::Info("grpc_server_started", {{"listen", listen_address_}});
  server->Wait();
}

}  // namespace app::transport::grpc
