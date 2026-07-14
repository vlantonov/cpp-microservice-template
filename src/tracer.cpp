#include "tracer.hpp"

#include <cstdio>
#include <stdexcept>
#include <string>

#include <grpcpp/grpcpp.h>

// OTel headers — ONLY included in this TU (PIMPL)
#include <opentelemetry/context/propagation/global_propagator.h>
#include <opentelemetry/context/propagation/text_map_propagator.h>
#include <opentelemetry/exporters/otlp/otlp_grpc_exporter_factory.h>
#include <opentelemetry/exporters/otlp/otlp_grpc_exporter_options.h>
#include <opentelemetry/sdk/resource/resource.h>
#include <opentelemetry/sdk/resource/semantic_conventions.h>
#include <opentelemetry/sdk/trace/batch_span_processor_factory.h>
#include <opentelemetry/sdk/trace/batch_span_processor_options.h>
#include <opentelemetry/sdk/trace/tracer_provider.h>
#include <opentelemetry/sdk/trace/tracer_provider_factory.h>
#include <opentelemetry/trace/canonical_code.h>
#include <opentelemetry/trace/propagation/http_trace_context.h>
#include <opentelemetry/trace/provider.h>
#include <opentelemetry/trace/span.h>
#include <opentelemetry/trace/span_context.h>
#include <opentelemetry/trace/tracer.h>

namespace hello {

// ---------------------------------------------------------------------------
// W3C traceparent carrier for gRPC client metadata
// ---------------------------------------------------------------------------
namespace {

class GrpcMetadataCarrier
    : public opentelemetry::context::propagation::TextMapCarrier {
public:
    explicit GrpcMetadataCarrier(const grpc::ServerContext* ctx) : ctx_(ctx) {}

    opentelemetry::nostd::string_view Get(
        opentelemetry::nostd::string_view key) const noexcept override {
        if (ctx_ == nullptr) { return {}; }
        const auto& md = ctx_->client_metadata();
        const auto  it = md.find(grpc::string_ref(key.data(), key.size()));
        if (it == md.end()) { return {}; }
        return {it->second.data(), it->second.size()};
    }

    void Set(opentelemetry::nostd::string_view /*key*/,
             opentelemetry::nostd::string_view /*value*/) noexcept override {
        // Server-side carrier: writes are not needed.
    }

private:
    const grpc::ServerContext* ctx_;
};

}  // namespace

// ---------------------------------------------------------------------------
// PIMPL structs
// ---------------------------------------------------------------------------

struct Tracer::SpanHandle::Impl {
    opentelemetry::nostd::shared_ptr<opentelemetry::trace::Span> span;
};

struct Tracer::Impl {
    opentelemetry::nostd::shared_ptr<opentelemetry::trace::TracerProvider> provider;
    opentelemetry::nostd::shared_ptr<opentelemetry::trace::Tracer>         tracer;
    std::string service_name;
};

// ---------------------------------------------------------------------------
// SpanHandle special members (defined here where Impl is complete)
// ---------------------------------------------------------------------------

Tracer::SpanHandle::SpanHandle()                                = default;
Tracer::SpanHandle::~SpanHandle()                               = default;
Tracer::SpanHandle::SpanHandle(SpanHandle&&) noexcept           = default;
Tracer::SpanHandle& Tracer::SpanHandle::operator=(SpanHandle&&) noexcept = default;

// ---------------------------------------------------------------------------
// Tracer constructor / destructor
// ---------------------------------------------------------------------------

Tracer::Tracer(std::string_view service_name, std::string_view otlp_endpoint)
    : impl_(std::make_unique<Impl>()) {
    impl_->service_name = std::string(service_name);

    try {
        opentelemetry::exporter::otlp::OtlpGrpcExporterOptions opts;
        opts.endpoint            = std::string(otlp_endpoint);
        opts.use_ssl_credentials = false;  // h2c — cleartext

        auto exporter = opentelemetry::exporter::otlp::OtlpGrpcExporterFactory::Create(opts);

        opentelemetry::sdk::trace::BatchSpanProcessorOptions batch_opts{};
        auto processor = opentelemetry::sdk::trace::BatchSpanProcessorFactory::Create(
            std::move(exporter), batch_opts);

        auto resource = opentelemetry::sdk::resource::Resource::Create({
            {opentelemetry::sdk::resource::SemanticConventions::kServiceName,
             std::string(service_name)},
        });

        auto sdk_provider = opentelemetry::sdk::trace::TracerProviderFactory::Create(
            std::move(processor), resource);

        // OTel SDK uses nostd::shared_ptr which has no make_shared equivalent;
        // sdk_provider.release() transfers unique_ptr ownership to nostd::shared_ptr.
        impl_->provider = opentelemetry::nostd::shared_ptr<opentelemetry::trace::TracerProvider>(
            sdk_provider.release());

        opentelemetry::trace::Provider::SetTracerProvider(impl_->provider);

        // Install W3C HTTP trace-context propagator.
        // nostd::shared_ptr has no make_shared — construction via raw new is the
        // OTel SDK's prescribed pattern for this interface.
        opentelemetry::context::propagation::GlobalTextMapPropagator::SetGlobalPropagator(
            opentelemetry::nostd::shared_ptr<opentelemetry::context::propagation::TextMapPropagator>(
                new opentelemetry::trace::propagation::HttpTraceContext()));

        impl_->tracer = impl_->provider->GetTracer(std::string(service_name));

    } catch (const std::exception& ex) {
        // FR-16: tracing failures must not affect request handling.
        // Fall back to the global no-op provider.
        std::fprintf(stderr,
                     "[hello::Tracer WARN] Failed to initialise OTel provider: %s."
                     " Tracing will be a no-op.\n",
                     ex.what());
        impl_->provider = opentelemetry::trace::Provider::GetTracerProvider();
        impl_->tracer   = impl_->provider->GetTracer(std::string(service_name));
    }
}

Tracer::~Tracer() {
    if (!impl_) { return; }
    // Cast back to SDK provider to call ForceFlush/Shutdown (§6.4)
    auto* sdk_provider =
        dynamic_cast<opentelemetry::sdk::trace::TracerProvider*>(impl_->provider.get());
    if (sdk_provider != nullptr) {
        sdk_provider->ForceFlush();
        sdk_provider->Shutdown();
    }
}

// ---------------------------------------------------------------------------
// Tracer::StartSpan
// ---------------------------------------------------------------------------

Tracer::SpanHandle Tracer::StartSpan(
    std::string_view name,
    const grpc::ServerContext* grpc_ctx,
    const std::map<std::string, std::string>& attributes) {

    // Extract W3C traceparent from gRPC metadata (may be empty).
    GrpcMetadataCarrier carrier(grpc_ctx);
    auto ctx = opentelemetry::context::propagation::GlobalTextMapPropagator::
        GetGlobalPropagator()->Extract(carrier,
            opentelemetry::context::RuntimeContext::GetCurrent());

    opentelemetry::trace::StartSpanOptions opts;
    opts.parent = ctx;

    auto span = impl_->tracer->StartSpan(std::string(name), {}, opts);

    for (const auto& [k, v] : attributes) {
        span->SetAttribute(k, v);
    }

    SpanHandle handle;
    handle.impl = std::make_unique<SpanHandle::Impl>();
    handle.impl->span = span;

    // Extract trace-id as 32-char lowercase hex string.
    const auto span_ctx = span->GetContext();
    if (span_ctx.IsValid()) {
        char buf[32] = {};
        span_ctx.trace_id().ToLowerBase16(
            opentelemetry::nostd::span<char, 32>{buf, 32});
        handle.trace_id = std::string(buf, 32);
    }

    return handle;
}

// ---------------------------------------------------------------------------
// Tracer::EndSpan
// ---------------------------------------------------------------------------

void Tracer::EndSpan(SpanHandle& handle,
                     SpanStatus  status,
                     std::string_view description) {
    if (!handle.impl || !handle.impl->span) { return; }

    using OtelCode = opentelemetry::trace::StatusCode;
    OtelCode code  = OtelCode::kUnset;
    switch (status) {
        case SpanStatus::kOk:    code = OtelCode::kOk;    break;
        case SpanStatus::kError: code = OtelCode::kError;  break;
        default:                 code = OtelCode::kUnset;  break;
    }

    handle.impl->span->SetStatus(code, std::string(description));
    handle.impl->span->End();
}

}  // namespace hello
