#pragma once

#include <grpcpp/grpcpp.h>

#include "hello/v1/hello.grpc.pb.h"
#include "logger.hpp"
#include "metrics.hpp"
#include "tracer.hpp"

namespace hello {

/**
 * @brief gRPC service implementation for hello.v1.HelloService.
 *
 * Thread-safety: safe for concurrent calls from gRPC's default thread pool.
 * The instance holds only non-owning references to Logger, Metrics, and Tracer
 * whose lifetimes are managed exclusively by main() (§6.3).  No mutable state
 * is introduced; all observability calls delegate to thread-safe libraries.
 */
class HelloServiceImpl final : public hello::v1::HelloService::Service {
public:
    /**
     * @brief Constructs the service implementation.
     *
     * All three parameters are non-owning references.  The caller (main.cpp)
     * guarantees that Logger, Metrics, and Tracer outlive this object (§6.4).
     *
     * @param logger  Logger instance for structured JSON output.
     * @param metrics Metrics registry for counter/histogram updates.
     * @param tracer  Tracer for OTLP span emission.
     */
    HelloServiceImpl(Logger& logger, Metrics& metrics, Tracer& tracer);

    /**
     * @brief Handles a unary SayHello RPC.
     *
     * Starts an OTel span, increments rpc_requests_total, observes request
     * latency, and returns "Hello, <message>!".  Returns INVALID_ARGUMENT
     * if the request message field is empty.
     *
     * @param ctx   gRPC server context (used for W3C traceparent extraction).
     * @param req   Incoming HelloRequest.
     * @param resp  Outgoing HelloResponse populated on success.
     * @return grpc::Status::OK on success, or an error status.
     */
    grpc::Status SayHello(grpc::ServerContext* ctx,
                          const hello::v1::HelloRequest* req,
                          hello::v1::HelloResponse* resp) override;

    // StreamHello is defined in the proto but not implemented this iteration.
    // grpc::Status StreamHello(grpc::ServerContext*, const hello::v1::HelloRequest*,
    //     grpc::ServerWriter<hello::v1::HelloResponse>*) override;

private:
    Logger&  logger_;
    Metrics& metrics_;
    Tracer&  tracer_;
};

}  // namespace hello
