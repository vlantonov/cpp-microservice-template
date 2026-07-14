#pragma once

#include <map>
#include <memory>
#include <string>
#include <string_view>

// Forward declarations — keep OTel and gRPC headers out of consumers.
namespace grpc { class ServerContext; }

namespace hello {

/**
 * @brief Span completion status, mirroring opentelemetry::trace::StatusCode.
 *
 * @note Deviation from design doc §5.4: the design specifies
 *       opentelemetry::trace::StatusCode directly in the public interface.
 *       A local mirror enum is used instead to enforce PIMPL discipline —
 *       callers must not need to include OTel headers merely to call EndSpan().
 *       The values intentionally match the OTel specification (Unset=0, Ok=1,
 *       Error=2) so tracer.cpp can cast without a lookup table.
 */
enum class SpanStatus : int {
    kUnset = 0,  ///< Default, span status not set.
    kOk    = 1,  ///< Span completed successfully.
    kError = 2,  ///< Span completed with an error.
};

/**
 * @brief Distributed tracing wrapper backed by the OpenTelemetry C++ SDK.
 *
 * Uses PIMPL to prevent OTel SDK headers from propagating into consumer TUs.
 * If the OTLP/gRPC exporter cannot reach the collector, construction succeeds
 * and a warning is printed; all span operations become no-ops (FR-16).
 *
 * Thread-safety: StartSpan and EndSpan are safe to call concurrently from
 * multiple gRPC worker threads (OTel SDK tracer is thread-safe per spec).
 */
class Tracer {
public:
    /**
     * @brief Initialises the OTel TracerProvider with BatchSpanProcessor +
     *        OTLP/gRPC exporter.
     * @param service_name   Written to the service.name resource attribute.
     * @param otlp_endpoint  "host:port" of the OTel collector, e.g.
     *                       "otel-collector:4317".  If unreachable, tracing
     *                       silently degrades to no-ops.
     */
    Tracer(std::string_view service_name, std::string_view otlp_endpoint);

    /** @brief ForceFlush()es then Shuts down the TracerProvider. */
    ~Tracer();

    // Non-copyable, non-movable (owns global OTel provider registration).
    Tracer(const Tracer&)            = delete;
    Tracer& operator=(const Tracer&) = delete;
    Tracer(Tracer&&)                 = delete;
    Tracer& operator=(Tracer&&)      = delete;

    /**
     * @brief Opaque RAII span handle returned by StartSpan.
     *
     * The underlying span is ended (set as error / OK) by EndSpan().
     * Destroying the handle without calling EndSpan is safe — the span
     * is ended with SpanStatus::kUnset.
     */
    struct SpanHandle {
        struct Impl;
        std::unique_ptr<Impl> impl;   ///< Opaque; not accessed by callers.
        std::string trace_id;         ///< 32-char hex trace-id; empty if invalid.

        SpanHandle();
        ~SpanHandle();
        SpanHandle(SpanHandle&&) noexcept;
        SpanHandle& operator=(SpanHandle&&) noexcept;
        SpanHandle(const SpanHandle&)            = delete;
        SpanHandle& operator=(const SpanHandle&) = delete;
    };

    /**
     * @brief Starts a new span, propagating W3C traceparent from gRPC metadata.
     * @param name       Span name, e.g. "SayHello".
     * @param grpc_ctx   gRPC server context for traceparent extraction; may be
     *                   nullptr to create a root span.
     * @param attributes Optional key/value span attributes.
     * @return A SpanHandle whose lifetime controls the active span.
     */
    SpanHandle StartSpan(std::string_view name,
                         const grpc::ServerContext* grpc_ctx,
                         const std::map<std::string, std::string>& attributes = {});

    /**
     * @brief Ends the span carried by @p handle, setting its final status.
     * @param handle      SpanHandle returned by StartSpan.
     * @param status      Completion status.
     * @param description Optional human-readable status description.
     */
    void EndSpan(SpanHandle& handle,
                 SpanStatus  status,
                 std::string_view description = "");

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace hello
