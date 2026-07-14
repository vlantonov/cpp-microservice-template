#include "hello_service.hpp"

#include <chrono>
#include <string>

namespace hello {

HelloServiceImpl::HelloServiceImpl(Logger& logger, Metrics& metrics, Tracer& tracer)
    : logger_(logger), metrics_(metrics), tracer_(tracer) {}

grpc::Status HelloServiceImpl::SayHello(
    grpc::ServerContext* ctx,
    const hello::v1::HelloRequest* req,
    hello::v1::HelloResponse* resp) {

    const auto start = std::chrono::steady_clock::now();

    auto span = tracer_.StartSpan(
        "SayHello", ctx,
        {{"rpc.system",  "grpc"},
         {"rpc.service", "hello.v1.HelloService"},
         {"rpc.method",  "SayHello"}});

    if (req->message().empty()) {
        logger_.Warn("SayHello rejected: empty message field", span.trace_id);

        tracer_.EndSpan(span, SpanStatus::kError, "message field must not be empty");

        const double elapsed = std::chrono::duration<double>(
            std::chrono::steady_clock::now() - start).count();
        metrics_.RecordRequest("SayHello", "INVALID_ARGUMENT");
        metrics_.ObserveRequestDuration("SayHello", elapsed);

        return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT,
                            "message field must not be empty");
    }

    resp->set_message("Hello, " + req->message() + "!");

    tracer_.EndSpan(span, SpanStatus::kOk);

    const double elapsed = std::chrono::duration<double>(
        std::chrono::steady_clock::now() - start).count();
    metrics_.RecordRequest("SayHello", "OK");
    metrics_.ObserveRequestDuration("SayHello", elapsed);

    logger_.Info("SayHello completed", span.trace_id);

    return grpc::Status::OK;
}

}  // namespace hello
