# C++ Microservice Scaffold — Design Document

| Field        | Value                              |
|--------------|------------------------------------|
| Version      | 0.1.0                              |
| Date         | 2026-07-14                         |
| Status       | Approved                           |
| Author       | System Architect Agent             |
| Derived from | docs/requirements/microservice-srs.md |

---

## 1. Architecture Overview

### 1.1 Component Diagram

```
┌──────────────────────────────────────────────────────────────────────────────┐
│                          docker-compose network                               │
│                                                                               │
│  ┌──────────┐  HTTP/JSON     ┌──────────────────┐  gRPC h2c                  │
│  │  Client  │──────────────▶ │   Envoy Proxy    │─────────────────┐          │
│  │ (curl /  │   :8080        │ grpc_json_       │   :50051        │          │
│  │  browser)│                │ transcoder       │                 ▼          │
│  └──────────┘                │                  │  ┌──────────────────────┐  │
│                               │  GET /healthz    │  │    hello_server      │  │
│                               │─────────────────▶│  │  (HelloServiceImpl)  │  │
│                               │ direct_response  │  │                      │  │
│                               └──────────────────┘  │  ┌────────────────┐  │  │
│                                                      │  │  Logger        │  │  │
│                                                      │  │  (spdlog JSON) │  │  │
│                                                      │  └───────┬────────┘  │  │
│                                                      │          │ stdout     │  │
│  ┌────────────────────┐  OTLP/gRPC :4317             │  ┌───────▼────────┐  │  │
│  │   OTel Collector   │◀────────────────────────────-│  │  Tracer        │  │  │
│  │                    │                              │  │  (OTel SDK)    │  │  │
│  └────────────────────┘                              │  └────────────────┘  │  │
│                                                      │  ┌────────────────┐  │  │
│  ┌────────────────────┐  HTTP scrape :9090/metrics   │  │  Metrics       │  │  │
│  │   Prometheus       │◀────────────────────────────-│  │  (prom-cpp +   │  │  │
│  │                    │                              │  │   cpp-httplib) │  │  │
│  └────────────────────┘                              │  └────────────────┘  │  │
│                                                      └──────────────────────┘  │
│  stdout ──▶ container runtime log collector                                    │
└───────────────────────────────────────────────────────────────────────────────-┘
```

### 1.2 Key Data Flows

| Flow | Path |
|------|------|
| REST request | Client → Envoy `:8080` → gRPC transcoding → `hello_server` `:50051` |
| Health check | Client → Envoy `:8080/healthz` → `direct_response` (HTTP 200, no backend call) |
| Distributed traces | `hello_server` OTLP/gRPC exporter → OTel Collector `:4317` |
| Metrics scrape | Prometheus → HTTP GET `hello_server` `:9090/metrics` |
| Logs | `hello_server` stdout → container runtime log collector |

---

## 2. Module Breakdown

The service is decomposed into four focused CMake targets with clear ownership boundaries:

| Module | CMake Type | Responsibility |
|--------|-----------|----------------|
| `proto_gen` | STATIC library | Compiled protobuf/gRPC generated code |
| `hello_lib` | STATIC library | Business logic + observability wrappers |
| `hello_server` | EXECUTABLE | Server entry point and lifecycle management |
| `hello_tests` | EXECUTABLE | GoogleTest unit test suite |

### 2.1 `proto_gen` — Compiled protobuf/gRPC artefacts

Wraps `protoc`-generated code so that generated translation units are compiled exactly once and linked wherever needed.

**Generated files** (written to `${CMAKE_BINARY_DIR}/generated/`):
- `hello.pb.h` / `hello.pb.cc` — protobuf message types
- `hello.grpc.pb.h` / `hello.grpc.pb.cc` — gRPC stub and `Service` base class

**CMake dependencies:**
- `PUBLIC`: `protobuf::libprotobuf`, `gRPC::grpc++`
- `PUBLIC INCLUDE DIRECTORIES`: `${CMAKE_BINARY_DIR}/generated`

### 2.2 `hello_lib` — Service logic and observability

Contains all service logic and observability code. This is the primary unit-testable target; it carries no `main()` and can be linked by both `hello_server` and `hello_tests`.

**Source files** (in `src/`):
- `hello_service.hpp` / `hello_service.cpp` — `HelloServiceImpl`, gRPC health service registration
- `logger.hpp` / `logger.cpp` — `Logger`, JSON formatter, trace-id injection via spdlog
- `metrics.hpp` / `metrics.cpp` — `Metrics`, prometheus-cpp registry, cpp-httplib HTTP server
- `tracer.hpp` / `tracer.cpp` — `Tracer`, OTel SDK setup, span helpers, W3C context propagation

**CMake dependencies:**
- `PRIVATE`: `proto_gen`, `spdlog::spdlog`, `prometheus-cpp::core`, `httplib::httplib`, OTel SDK targets (`opentelemetry_api`, `opentelemetry_sdk`, `opentelemetry_exporter_otlp_grpc`)
- `PUBLIC`: `gRPC::grpc++` (callers need gRPC types such as `grpc::ServerContext`)

### 2.3 `hello_server` — Entry point

Contains `src/main.cpp` only. Constructs all root objects (`Logger`, `Metrics`, `Tracer`), builds the gRPC server, installs POSIX signal handlers, and blocks until shutdown.

**CMake dependencies:**
- `PRIVATE`: `hello_lib`, `gRPC::grpc++_reflection`

### 2.4 `hello_tests` — Unit test suite

One test file per production class. Linked against `hello_lib` and `GTest::gtest_main`; no dependency on `hello_server`.

**Source files** (in `tests/`):
- `hello_service_test.cpp`
- `logger_test.cpp`
- `metrics_test.cpp`
- `tracer_test.cpp`

**CMake dependencies:**
- `PRIVATE`: `hello_lib`, `proto_gen`, `GTest::gtest_main`

---

## 3. CMake Target Structure

### 3.1 Subdirectory layout

```
CMakeLists.txt (root)
│
├── Conan 2 dependencies (§7)
│
├── add_subdirectory(proto)    →  proto_gen  (STATIC)
│   └── Custom command: protoc + grpc_cpp_plugin → generated/*.pb.{h,cc}
│   └── Custom command: protoc --descriptor_set_out → ${CMAKE_BINARY_DIR}/proto.pb
│
├── add_subdirectory(src)      →  hello_lib  (STATIC)
│                              →  hello_server (EXECUTABLE)
│
└── add_subdirectory(tests)    →  hello_tests (EXECUTABLE)
    └── enable_testing()
    └── add_test(NAME hello_tests COMMAND hello_tests)
```

### 3.2 Target dependency graph

```
hello_server  ─PRIVATE─▶  hello_lib  ─PRIVATE─▶  proto_gen
     │                        │                       │
     └─PRIVATE─▶ grpc++_refl  ├─PRIVATE─▶ spdlog     ├─PUBLIC─▶ protobuf
                               ├─PRIVATE─▶ prom-cpp   └─PUBLIC─▶ grpc++
                               ├─PRIVATE─▶ httplib
                               └─PRIVATE─▶ opentelemetry-cpp

hello_tests   ─PRIVATE─▶  hello_lib
              ─PRIVATE─▶  proto_gen
              ─PRIVATE─▶  GTest::gtest_main
```

### 3.3 Root CMake variables

| Variable | Value | Rationale |
|----------|-------|-----------|
| `CMAKE_CXX_STANDARD` | `20` | NFR-4 |
| `CMAKE_CXX_STANDARD_REQUIRED` | `ON` | Prevent silent downgrade |
| `CMAKE_CXX_EXTENSIONS` | `OFF` | Portable, standards-conforming code |
| `CMAKE_EXPORT_COMPILE_COMMANDS` | `ON` | Required by `run-clang-tidy` in CI |

---

## 4. Directory Structure

```
.
├── CMakeLists.txt                         # Root: find_package() + add_subdirectory
├── conanfile.py                           # Conan 2 dependency manifest
├── CMakePresets.json                      # Base presets; extended by CMakeUserPresets.json
├── proto/
│   └── hello/v1/
│       └── hello.proto                    # Service definition + HTTP annotations
├── src/
│   ├── CMakeLists.txt                     # Defines hello_lib + hello_server
│   ├── main.cpp                           # Server entry point
│   ├── hello_service.hpp
│   ├── hello_service.cpp
│   ├── logger.hpp
│   ├── logger.cpp
│   ├── metrics.hpp
│   ├── metrics.cpp
│   ├── tracer.hpp
│   └── tracer.cpp
├── tests/
│   ├── CMakeLists.txt                     # Defines hello_tests
│   ├── hello_service_test.cpp
│   ├── logger_test.cpp
│   ├── metrics_test.cpp
│   └── tracer_test.cpp
├── deploy/
│   ├── envoy/
│   │   └── envoy.yaml
│   ├── otel-collector/
│   │   └── otel-collector-config.yaml
│   └── prometheus/
│       └── prometheus.yml
├── .clang-tidy                            # Checks config; excludes generated headers
├── CMakeUserPresets.json                  # Generated by `conan install` (gitignored)
├── Dockerfile
├── docker-compose.yml
├── .github/
│   └── workflows/
│       └── ci.yml
├── README.md
└── LICENSE                                # MIT
```

**Important notes:**
- Generated proto files (`*.pb.h`, `*.pb.cc`) land in `${CMAKE_BINARY_DIR}/generated/` — never committed to the source tree.
- `proto.pb` (Envoy descriptor set) is generated into `${CMAKE_BINARY_DIR}/proto.pb` and mounted into the Envoy container via a docker-compose volume.
- `CMakeUserPresets.json` is generated by `conan install` and is gitignored; it must be (re-)generated before `cmake --preset` will work.

---

## 5. Public Interfaces

All declarations live in `src/*.hpp`. No implementation logic appears in headers. PIMPL is applied selectively to `Metrics` and `Tracer` to prevent heavy SDK headers from propagating to every translation unit.

### 5.1 `HelloServiceImpl`

```cpp
// src/hello_service.hpp
#pragma once
#include <grpcpp/grpcpp.h>
#include "hello.grpc.pb.h"
#include "logger.hpp"
#include "metrics.hpp"
#include "tracer.hpp"

namespace hello {

class HelloServiceImpl final : public hello::v1::HelloService::Service {
public:
    // Dependencies injected by non-owning reference.
    // Caller (main.cpp) guarantees all three outlive this object.
    HelloServiceImpl(Logger& logger, Metrics& metrics, Tracer& tracer);

    grpc::Status SayHello(grpc::ServerContext* ctx,
                          const hello::v1::HelloRequest* req,
                          hello::v1::HelloResponse* resp) override;

    // Defined in proto; not implemented this iteration.
    // grpc::Status StreamHello(grpc::ServerContext* ctx,
    //                          const hello::v1::HelloRequest* req,
    //                          grpc::ServerWriter<hello::v1::HelloResponse>* writer) override;

private:
    Logger&  logger_;
    Metrics& metrics_;
    Tracer&  tracer_;
};

} // namespace hello
```

### 5.2 `Logger`

```cpp
// src/logger.hpp
#pragma once
#include <string_view>
#include <memory>
#include <spdlog/spdlog.h>

namespace hello {

class Logger {
public:
    // service_name: logged in every record as the "service" JSON field.
    // level: initial severity filter.
    Logger(std::string_view service_name, spdlog::level::level_enum level);

    // Emit a JSON log record. trace_id must be a 32-char hex OTel trace ID
    // or an empty string when no active span exists.
    void Debug(std::string_view message, std::string_view trace_id = "");
    void Info (std::string_view message, std::string_view trace_id = "");
    void Warn (std::string_view message, std::string_view trace_id = "");
    void Error(std::string_view message, std::string_view trace_id = "");

    // Reads LOG_LEVEL env var; returns spdlog::level::info if absent or invalid.
    static spdlog::level::level_enum LevelFromEnv();

private:
    std::shared_ptr<spdlog::logger> impl_;
    std::string service_name_;
};

} // namespace hello
```

**JSON record schema** (emitted via spdlog `pattern_formatter`):
```json
{
  "timestamp": "2026-07-14T12:00:00.000Z",
  "severity": "INFO",
  "service": "hello",
  "trace_id": "4bf92f3577b34da6a3ce929d0e0e4736",
  "message": "RPC SayHello completed"
}
```

### 5.3 `Metrics`

PIMPL is used here to prevent prometheus-cpp's macro-heavy headers from leaking into every translation unit that includes `metrics.hpp`.

```cpp
// src/metrics.hpp
#pragma once
#include <cstdint>
#include <string_view>
#include <memory>

namespace hello {

class Metrics {
public:
    // Registers Counter/Histogram/Gauge families with internal prometheus::Registry.
    // http_port: port for the embedded cpp-httplib server (default 9090).
    Metrics(std::string_view service_name, uint16_t http_port);
    ~Metrics();

    // All three methods are thread-safe; prometheus-cpp uses internal atomics.
    void RecordRequest(std::string_view method, std::string_view status);
    void ObserveRequestDuration(std::string_view method, double duration_seconds);
    void SetActiveConnections(double count);

    // Starts the cpp-httplib server; blocks until Shutdown() is called.
    // Caller must invoke this on a dedicated std::thread.
    void ServeForever();

    // Signals the httplib server to stop; ServeForever() returns.
    void Shutdown();

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace hello
```

**Metric definitions** (in `metrics.cpp`):

| Name | Type | Labels | Description |
|------|------|--------|-------------|
| `rpc_requests_total` | Counter | `method`, `status`, `service` | Total RPC calls received |
| `rpc_request_duration_seconds` | Histogram | `method`, `service` | RPC handler latency |
| `rpc_active_connections` | Gauge | `service` | Currently active gRPC connections |

### 5.4 `Tracer`

PIMPL prevents OTel SDK headers from propagating to callers.

```cpp
// src/tracer.hpp
#pragma once
#include <string_view>
#include <map>
#include <memory>
#include <string>

namespace grpc { class ServerContext; }
namespace opentelemetry::trace { enum class StatusCode; }

namespace hello {

class Tracer {
public:
    // Initialises OTel TracerProvider with BatchSpanProcessor + OTLP/gRPC exporter.
    // otlp_endpoint: "host:port", e.g. "otel-collector:4317"
    // If the endpoint is unreachable, construction succeeds and a warning is logged
    // (FR-16: tracing failures must not affect request handling).
    Tracer(std::string_view service_name, std::string_view otlp_endpoint);

    // RAII: calls ForceFlush() then ShutDown() on the TracerProvider.
    ~Tracer();

    struct SpanHandle {
        // opaque internal span and context; never accessed directly by callers.
        struct Impl;
        std::unique_ptr<Impl> impl;

        std::string trace_id; // 32-char hex; empty if span is invalid.
    };

    // Extracts W3C traceparent from gRPC metadata headers in grpc_ctx (may be nullptr).
    // Creates a child span if a valid parent context is found; otherwise a root span.
    SpanHandle StartSpan(std::string_view name,
                         const grpc::ServerContext* grpc_ctx,
                         const std::map<std::string, std::string>& attributes = {});

    void EndSpan(SpanHandle& handle,
                 opentelemetry::trace::StatusCode status,
                 std::string_view description = "");

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace hello
```

---

## 6. Key Design Decisions

### 6.1 Why Envoy for REST transcoding

Envoy's `grpc_json_transcoder` filter was chosen over `grpc-gateway` (Go-only; incompatible with a C++ service project) and a hand-rolled HTTP layer (extra C++ maintenance burden, duplicates REST routing logic). Envoy fully decouples the HTTP/JSON surface from the service implementation: adding or modifying REST routes requires only changes to `envoy.yaml` and the proto HTTP annotations, with zero C++ source changes. This mirrors a realistic production deployment topology — any team adopting this scaffold in a Kubernetes environment will already operate Envoy or an Envoy-based mesh (Istio, Contour). The only added complexity is generating and mounting the proto descriptor set (`proto.pb`) into the Envoy container, which is a single CMake custom command and one docker-compose volume entry.

### 6.2 Why prometheus-cpp (not OTel metrics SDK) for FR-17/FR-18

Using the OTel metrics SDK to export to Prometheus would require a Prometheus receiver in an OTel Collector sidecar, adding the dependency chain `service → Collector → Prometheus`. If the collector is unavailable, metrics disappear. `prometheus-cpp` produces the Prometheus text exposition format directly from the service process, making metrics independently accessible at `:9090/metrics` with no intermediate hop. This resolves OQ-2 by keeping the metrics and tracing pipelines independent and reduces integration risk. The trade-off is that instrumentation is spread across two SDKs (OTel for traces, prometheus-cpp for metrics) rather than one; this is acceptable for a scaffold where operational simplicity and reliability outweigh SDK unification.

### 6.3 Thread-safety strategy (NFR-13)

`HelloServiceImpl` is instantiated once and shared across gRPC's default thread pool. Thread safety is achieved at three levels: (1) `Logger` wraps spdlog, which guarantees thread-safe sink writes via internal mutexes. (2) `Metrics` wraps prometheus-cpp's `Registry`, which uses atomic operations for Counter increments and Gauge/Histogram updates. (3) `Tracer` wraps the OTel C++ SDK `Tracer` interface, which is thread-safe per the OTel specification. `HelloServiceImpl` itself holds only non-owning references (`Logger&`, `Metrics&`, `Tracer&`) to stable objects with lifetimes managed exclusively by `main()`; no synchronization is required at the service level. No global mutable state is introduced anywhere; environment variable reads occur exactly once at startup, before the gRPC server starts accepting connections.

### 6.4 RAII ownership model for server startup/shutdown

All root objects (`Logger`, `Metrics`, `Tracer`) are constructed as local variables in `main()` before the gRPC server is built. `HelloServiceImpl` receives them by non-owning reference and does not participate in their lifetime. The `grpc::Server` is held as a `std::unique_ptr<grpc::Server>`; a POSIX signal handler trampoline for `SIGINT`/`SIGTERM` calls `server->Shutdown()`, which drains in-flight RPCs before returning. The metrics HTTP thread (`std::thread`) is joined after `Metrics::Shutdown()` signals the httplib server to stop. `Tracer`'s destructor calls `ForceFlush()` (drains pending spans) then `ShutDown()` on the provider. Destruction order is LIFO relative to declaration order in `main()`: gRPC server shuts down first (no new RPCs), then metrics HTTP server, then tracer (flushes spans), then logger. This ordering guarantees no use-after-free across the shutdown path and ensures all telemetry is flushed before the process exits.

---

## 7. Conan 2 Dependency Manifest

All third-party code is resolved at `conan install` time. No system-installed libraries are required beyond a C++ toolchain, CMake, and Conan 2.

### 7.1 Declarations

| Conan reference | Purpose |
|-----------------|---------|
| `grpc/1.69.0` | gRPC runtime, protobuf, `grpc_cpp_plugin` |
| *(vendored)* `proto/third_party/googleapis` | `google/api/annotations.proto` + `http.proto` for HTTP bindings; googleapis CCI recipe pins `protobuf/3.21.12` which conflicts with gRPC's `protobuf/5.27.0` |
| `spdlog/1.14.1` | Structured JSON logging |
| `prometheus-cpp/1.2.4` | Prometheus metrics registry + text serializer |
| `opentelemetry-cpp/1.21.0` | OTel SDK + OTLP/gRPC exporter |
| `cpp-httplib/0.16.3` | Embedded HTTP server for `/metrics` endpoint |
| `gtest/1.14.0` | Unit test framework |

### 7.2 Conan option overrides (set in `conanfile.py` `configure()`)

| Dependency | Option | Value | Reason |
|------------|--------|-------|--------|
| `prometheus-cpp` | `with_pull` | `False` | Core library only; no civetweb-based Exposer |
| `prometheus-cpp` | `with_push` | `False` | Push gateway not needed |
| `grpc` | `cpp_plugin` | `True` | Required for proto code generation |
| `opentelemetry-cpp` | `with_otlp_grpc` | `True` | Enable OTLP/gRPC exporter (FR-15) |
| `opentelemetry-cpp` | `with_abseil` | `True` | Use same Abseil instance as gRPC |

---

## 8. Proto File Design

**`proto/hello/v1/hello.proto`** (structure and annotations — authoritative declaration, not generated):

```proto
syntax = "proto3";

package hello.v1;

import "google/api/annotations.proto";

// HelloService provides greeting RPCs.
service HelloService {

  // SayHello: unary RPC.
  // REST binding: GET /v1/hello?message=<msg>
  rpc SayHello(HelloRequest) returns (HelloResponse) {
    option (google.api.http) = {
      get: "/v1/hello"
    };
  }

  // StreamHello: server-side streaming.
  // Defined per FR-1; not implemented this iteration.
  rpc StreamHello(HelloRequest) returns (stream HelloResponse);
}

message HelloRequest {
  string message = 1;
}

message HelloResponse {
  string message = 1;
}
```

**Proto descriptor generation** — The `proto` CMake subdirectory must define a custom command:

```
protoc
  --proto_path=<GOOGLEAPIS_PROTO_PATH>
  --proto_path=${CMAKE_SOURCE_DIR}/proto
  --descriptor_set_out=${CMAKE_BINARY_DIR}/proto.pb
  --include_imports
  hello/v1/hello.proto
```

The generated `${CMAKE_BINARY_DIR}/proto.pb` is mounted read-only into the Envoy container at `/etc/envoy/proto.pb` via a docker-compose volume binding.

---

## 9. Envoy Configuration Sketch

**`deploy/envoy/envoy.yaml`** — authoritative key sections (full file is the developer's deliverable):

```yaml
static_resources:

  listeners:
    - name: listener_rest
      address:
        socket_address: { address: 0.0.0.0, port_value: 8080 }
      filter_chains:
        - filters:
            - name: envoy.filters.network.http_connection_manager
              typed_config:
                "@type": type.googleapis.com/envoy.extensions.filters.network.http_connection_manager.v3.HttpConnectionManager
                stat_prefix: ingress_http
                codec_type: AUTO
                route_config:
                  name: local_route
                  virtual_hosts:
                    - name: local_service
                      domains: ["*"]
                      routes:
                        # /healthz: answered immediately by Envoy (AC-6)
                        - match: { path: "/healthz" }
                          direct_response:
                            status: 200
                            body:
                              inline_string: '{"status":"ok"}'
                        # All other paths: transcoded to gRPC backend
                        - match: { prefix: "/" }
                          route: { cluster: grpc_hello }
                http_filters:
                  - name: envoy.filters.http.grpc_json_transcoder
                    typed_config:
                      "@type": type.googleapis.com/envoy.extensions.filters.http.grpc_json_transcoder.v3.GrpcJsonTranscoder
                      proto_descriptor: /etc/envoy/proto.pb   # mounted from build output
                      services:
                        - hello.v1.HelloService
                      print_options:
                        add_whitespace: true
                        always_print_primitive_fields: true
                        preserve_proto_field_names: false
                  - name: envoy.filters.http.router
                    typed_config:
                      "@type": type.googleapis.com/envoy.extensions.filters.http.router.v3.Router

  clusters:
    - name: grpc_hello
      connect_timeout: 5s
      type: LOGICAL_DNS
      dns_lookup_family: V4_ONLY
      lb_policy: ROUND_ROBIN
      typed_extension_protocol_options:
        envoy.extensions.upstreams.http.v3.HttpProtocolOptions:
          "@type": type.googleapis.com/envoy.extensions.upstreams.http.v3.HttpProtocolOptions
          explicit_http_config:
            http2_protocol_options: {}        # Force h2c (cleartext HTTP/2) to backend
      load_assignment:
        cluster_name: grpc_hello
        endpoints:
          - lb_endpoints:
              - endpoint:
                  address:
                    socket_address:
                      address: hello          # docker-compose service name
                      port_value: 50051
```

**`/healthz` design decision:** The `direct_response` satisfies AC-6 (`curl http://localhost:8080/healthz` → HTTP 200 `{"status":"ok"}`) without requiring a backend call. The gRPC health protocol (FR-3) is still implemented in `HelloServiceImpl` for use by infrastructure tooling (Kubernetes liveness probes, Envoy active health checks); those tools call `:50051` directly, not through this Envoy listener.

**Envoy image pinning:** Use `envoyproxy/envoy:v1.31.0` (or later stable release) in `docker-compose.yml`. Never use `:latest` (C-5).

---

## 10. CI Pipeline Design

**`.github/workflows/ci.yml`** — authoritative job structure (YAML is the developer's deliverable):

```
Trigger:
  push → main
  pull_request → main

Job: build-and-test
  runs-on: ubuntu-24.04
  strategy:
    matrix:
      build_type: [Debug, Release]

  Steps:

  1. actions/checkout@v4
     Fetch full repository.

  2. Install system packages (sudo apt-get)
     Packages: cmake, ninja-build, clang, clang-tidy, python3-pip,
               libssl-dev, pkg-config, zlib1g-dev
     Rationale: libssl-dev/zlib1g-dev are retained because some Conan
                recipes build against system OpenSSL/zlib when
                --build=missing is used.

  3. Install Conan 2
     pip3 install "conan>=2.0" && conan profile detect --force

  4. Cache Conan package store
     uses: actions/cache@v4
     key: conan-${{ runner.os }}-${{ hashFiles('conanfile.py') }}
     path: ~/.conan2/p

  5. Conan install
     conan install . --build=missing
       -s build_type=${{ matrix.build_type }}
       -s compiler=clang -s compiler.version=18
       -s compiler.libcxx=libstdc++11 -s compiler.cppstd=20
     Generates build/<build_type>/generators/conan_toolchain.cmake
     and CMakeUserPresets.json in the project root.

  6. CMake Configure
     cmake --preset conan-<debug|release>
     CMAKE_CXX_CLANG_TIDY is NOT set here (see step 9).

  7. Build
     cmake --build --preset conan-<debug|release> --parallel

  8. Test (all build types)
     ctest --test-dir build/${{ matrix.build_type }}
           --output-on-failure
           --parallel $(nproc)

  9. clang-tidy (Debug matrix variant only)
     Condition: matrix.build_type == 'Debug'
     Command: run-clang-tidy -p build/Debug src/
     Config: .clang-tidy at repo root
       Checks: '-*,readability-*,modernize-*,cppcoreguidelines-*,performance-*'
       HeaderFilterRegex: '^.*/src/.*'     # exclude generated proto and third-party headers
     Rationale: Running clang-tidy via CMAKE_CXX_CLANG_TIDY would analyze generated
                proto TUs, producing thousands of false positives. A separate
                run-clang-tidy step scoped to src/ is cleaner and faster.
```

**Caching rationale:** The Conan package store (`~/.conan2/p`) is cached keyed on `conanfile.py`'s hash so it invalidates automatically when any dependency version changes. Each matrix build_type gets its own toolchain generated under `build/<build_type>/generators/`.

---

## 11. Open Questions — Resolution Record

| # | Question | Resolution |
|---|----------|------------|
| OQ-1 | Canonical service/domain name | **`hello`** — proto package `hello.v1`, log `"service": "hello"`, metric label `service="hello"`, docker-compose service name `hello` |
| OQ-2 | Prometheus client library | **`prometheus-cpp`** — independent of OTel; direct text exposition; no Collector hop for metrics; reduces pipeline dependency risk |
| OQ-3 | `/metrics` HTTP server | **`cpp-httplib` embedded** in the service process on port `9090`; prometheus-cpp's civetweb-based pull Exposer is disabled (`ENABLE_PULL=OFF`); no Envoy config changes required |
| OQ-4 | ARM64 requirement | **Not required for this iteration** (C-1: Linux x86-64); `ubuntu:24.04` supports both architectures but only amd64 is tested in CI |
| OQ-5 | License | **MIT** — compatible with all dependencies: gRPC (Apache-2.0), spdlog (MIT), prometheus-cpp (Apache-2.0), OTel C++ SDK (Apache-2.0), GoogleTest (BSD-3-Clause), cpp-httplib (MIT) |

---

## 12. Risks and Mitigations

| Risk | Likelihood | Impact | Mitigation |
|------|-----------|--------|-----------|
| First `conan install --build=missing` can be slow when no binary packages are cached | Medium | Medium | CI caches `~/.conan2/p` keyed on `conanfile.py` hash; Docker layer cache reuses the Conan install layer |
| OTel C++ SDK version incompatible with Conan gRPC version (shared Abseil/protobuf) | Medium | High | Pin both to tested-compatible releases; set `with_abseil=True` so OTel uses the same Abseil instance as gRPC |
| Envoy `grpc_json_transcoder` fails because `proto.pb` descriptor does not match the running binary | High | High | Always generate `proto.pb` in the same CMake invocation that compiles the service binary; mount via docker-compose volume (never bake into the Envoy image) |
| Health proto not included in `proto.pb` descriptor, causing Envoy health check failures | Medium | Medium | Add `grpc/health/v1/health.proto` to the `protoc --descriptor_set_out` invocation |
| prometheus-cpp `ENABLE_PULL=OFF` means the pull module's `Exposer` is unavailable; `TextSerializer` must be used directly | Low | Low | Documented in §5.3; developer must serialize via `prometheus::TextSerializer` in `metrics.cpp` and hand the string to the cpp-httplib handler |
| PIMPL in `Metrics` and `Tracer` adds compile-time overhead for the Impl struct | Low | Low | Accepted trade-off for clean public headers; performance impact is negligible |

---

## 13. Handoff Statement

Design is approved. Ready for the **C++ Developer agent** to begin implementation.

Recommended implementation order to minimise blocking dependencies:

1. `proto/hello/v1/hello.proto` + `proto/CMakeLists.txt` → `proto_gen` target and `proto.pb` generation
2. `src/logger.hpp` / `logger.cpp` + `logger_test.cpp` (no SDK deps beyond spdlog; fastest feedback loop)
3. `src/metrics.hpp` / `metrics.cpp` + `metrics_test.cpp`
4. `src/tracer.hpp` / `tracer.cpp` + `tracer_test.cpp`
5. `src/hello_service.hpp` / `hello_service.cpp` + `hello_service_test.cpp`
6. `src/main.cpp` → `hello_server` executable
7. `Dockerfile` (multi-stage: ubuntu:24.04 build → distroless/cc-debian12 final)
8. `docker-compose.yml` + `deploy/envoy/envoy.yaml` + `deploy/otel-collector/` + `deploy/prometheus/`
9. `.github/workflows/ci.yml`
10. `README.md`
