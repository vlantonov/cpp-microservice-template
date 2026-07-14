#include <gtest/gtest.h>

#include <string>

#include "tracer.hpp"

namespace {

// Use a deliberately unreachable endpoint to exercise the graceful no-op path.
constexpr std::string_view kUnreachableEndpoint = "localhost:19317";

// ---------------------------------------------------------------------------
// Construction — must succeed even when collector is unreachable (FR-16)
// ---------------------------------------------------------------------------

TEST(TracerTest, ConstructsWithUnreachableEndpoint) {
    EXPECT_NO_THROW(hello::Tracer("test", kUnreachableEndpoint));
}

// ---------------------------------------------------------------------------
// SpanHandle
// ---------------------------------------------------------------------------

TEST(TracerTest, StartSpanReturnsHandle) {
    hello::Tracer tracer("test", kUnreachableEndpoint);
    const auto handle = tracer.StartSpan("TestSpan", /*grpc_ctx=*/nullptr);
    // Handle is valid whether or not the collector is reachable.
    EXPECT_NO_THROW((void)handle.trace_id);
}

TEST(TracerTest, TraceIdIsEmptyOrThirtyTwoHexChars) {
    hello::Tracer tracer("test", kUnreachableEndpoint);
    const auto handle = tracer.StartSpan("TestSpan", nullptr);
    const auto& tid   = handle.trace_id;
    if (!tid.empty()) {
        EXPECT_EQ(tid.size(), 32u);
        for (char c : tid) {
            EXPECT_TRUE((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))
                << "Non-hex character '" << c << "' in trace_id";
        }
    }
}

// ---------------------------------------------------------------------------
// EndSpan — all status codes
// ---------------------------------------------------------------------------

TEST(TracerTest, EndSpanOkDoesNotThrow) {
    hello::Tracer tracer("test", kUnreachableEndpoint);
    auto handle = tracer.StartSpan("OkSpan", nullptr);
    EXPECT_NO_THROW(tracer.EndSpan(handle, hello::SpanStatus::kOk));
}

TEST(TracerTest, EndSpanErrorDoesNotThrow) {
    hello::Tracer tracer("test", kUnreachableEndpoint);
    auto handle = tracer.StartSpan("ErrSpan", nullptr);
    EXPECT_NO_THROW(
        tracer.EndSpan(handle, hello::SpanStatus::kError, "something went wrong"));
}

TEST(TracerTest, EndSpanUnsetDoesNotThrow) {
    hello::Tracer tracer("test", kUnreachableEndpoint);
    auto handle = tracer.StartSpan("UnsetSpan", nullptr);
    EXPECT_NO_THROW(tracer.EndSpan(handle, hello::SpanStatus::kUnset));
}

// ---------------------------------------------------------------------------
// Multiple spans on the same tracer (thread-pool simulation)
// ---------------------------------------------------------------------------

TEST(TracerTest, MultipleSequentialSpans) {
    hello::Tracer tracer("test", kUnreachableEndpoint);
    for (int i = 0; i < 5; ++i) {
        auto h = tracer.StartSpan("Loop", nullptr, {{"iteration", std::to_string(i)}});
        tracer.EndSpan(h, hello::SpanStatus::kOk);
    }
}

// ---------------------------------------------------------------------------
// SpanHandle is moveable
// ---------------------------------------------------------------------------

TEST(TracerTest, SpanHandleIsMoveConstructible) {
    hello::Tracer tracer("test", kUnreachableEndpoint);
    auto h1 = tracer.StartSpan("Moveable", nullptr);
    hello::Tracer::SpanHandle h2(std::move(h1));
    // h2 carries the span; h1 is in a valid but unspecified state.
    EXPECT_NO_THROW(tracer.EndSpan(h2, hello::SpanStatus::kOk));
}

}  // namespace
