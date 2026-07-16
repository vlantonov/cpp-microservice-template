# Software Requirements Specification  
## Production-Grade C++ Microservice Scaffold  

| Field        | Value                         |
|--------------|-------------------------------|
| Version      | 0.1.0                         |
| Date         | 2026-07-14                    |
| Status       | Approved                      |
| Author       | Requirements Analyst Agent    |

---

## 1. Overview

This document specifies the requirements for a **production-grade C++ microservice scaffold** whose primary purpose is to demonstrate C++ as a viable, first-class backend language. The scaffold provides a reusable starting point for new microservices with gRPC as the primary transport, an HTTP/JSON REST gateway, structured logging, distributed tracing, and Prometheus metrics — all wired together and buildable from a single CMake invocation.

The scaffold is intended as a portfolio and reference project. Every technology choice listed here must be demonstrable end-to-end via a local `docker compose up` or equivalent single-command workflow.

---

## 2. Functional Requirements

### 2.1 gRPC Service

**FR-1** The scaffold shall define at least one example RPC in a Protocol Buffers (`.proto`) file. A minimal `Hello`/`Echo` unary RPC (request: string message → response: string message) is the baseline; an additional server-side streaming variant of the same RPC shall be noted in the proto file and documented as optional for this iteration.

**FR-2** The gRPC server shall listen on a configurable TCP port (default: `50051`) specified via an environment variable or configuration file.

**FR-3** The gRPC service shall implement the gRPC Health Checking Protocol (`grpc.health.v1.Health`) so that infrastructure tooling (e.g., Kubernetes liveness probes, Envoy health checks) can assess service readiness.

**FR-4** The service implementation shall return a well-formed gRPC error status (non-`OK`) when it receives a request it cannot process, rather than crashing or returning a partial response.

### 2.2 REST Gateway

**FR-5** The scaffold shall expose an HTTP/1.1 JSON REST interface that transcodes to and from the gRPC service defined in FR-1.

**FR-6** The chosen transcoding approach shall be **Envoy proxy** configured with gRPC-JSON transcoding (using the `envoy.filters.http.grpc_json_transcoder` filter). This approach is selected because:
  - It decouples the REST surface from the C++ service code entirely.
  - It requires no additional C++ library (`grpc-gateway` is Go-only).
  - It mirrors a realistic production deployment topology.

**FR-7** The Envoy configuration shall be included in the repository as a versioned file (e.g., `deploy/envoy/envoy.yaml`) and shall expose the transcoded REST API on port `8080` by default.

**FR-8** A `/healthz` HTTP endpoint shall be provided through Envoy (or a minimal embedded HTTP handler) and shall return HTTP `200 OK` with body `{"status":"ok"}` when the service is healthy.

### 2.3 Structured Logging

**FR-9** All log output shall be formatted as newline-delimited JSON. Each log record shall include at minimum the following fields:

| Field         | Description                              |
|---------------|------------------------------------------|
| `timestamp`   | ISO 8601 UTC timestamp                   |
| `severity`    | One of: `DEBUG`, `INFO`, `WARN`, `ERROR` |
| `service`     | Static service name string               |
| `trace_id`    | OpenTelemetry trace ID (hex, 32 chars) when available; empty string otherwise |
| `message`     | Human-readable log message               |

**FR-10** The logging library shall be **spdlog** (version ≥ 1.12). Severity level shall be configurable at runtime via an environment variable (e.g., `LOG_LEVEL=debug`).

**FR-11** Log records shall be written to `stdout` so that container runtimes and log aggregators (Fluentd, Loki, CloudWatch, etc.) can collect them without file-path configuration.

### 2.4 Distributed Tracing

**FR-12** The service shall integrate the **OpenTelemetry C++ SDK** (version ≥ 1.14) to instrument gRPC request handling.

**FR-13** A trace span shall be created for each inbound RPC call. The span shall record at minimum:
  - RPC method name
  - gRPC status code on completion
  - Any error message on non-`OK` status

**FR-14** The service shall extract trace context from incoming gRPC metadata (W3C `traceparent`/`tracestate` headers or gRPC binary propagation) and propagate it as the parent span.

**FR-15** Completed traces shall be exported via **OTLP over gRPC** (default collector endpoint: `localhost:4317`, configurable via `OTEL_EXPORTER_OTLP_ENDPOINT` environment variable). OTLP over HTTP shall be noted as an alternative exporter configuration.

**FR-16** When no collector is reachable the service shall log a warning and continue operating — tracing failures must not affect request handling.

### 2.5 Prometheus Metrics

**FR-17** The service shall expose a `/metrics` endpoint over plain HTTP (default port: `9090`, configurable via environment variable) in Prometheus text exposition format (version 0.0.4 or OpenMetrics).

**FR-18** The following metrics shall be present at minimum:

| Metric name                        | Type      | Description                                      |
|------------------------------------|-----------|--------------------------------------------------|
| `rpc_requests_total`               | Counter   | Total RPC calls received, labelled by `method` and `status` |
| `rpc_request_duration_seconds`     | Histogram | RPC handler latency in seconds, labelled by `method` |
| `rpc_active_connections`           | Gauge     | Number of currently active gRPC connections      |

**FR-19** All metrics shall include a `service` label set to the service name string.

---

## 3. Non-Functional Requirements

### 3.1 Build System

**NFR-1** The project shall use **CMake ≥ 3.23** as its sole build system.

**NFR-2** Third-party dependencies shall be managed via **Conan 2** (`conanfile.py`). All dependency versions are pinned in `conanfile.py`; contributors run `conan install . --build=missing` once before invoking CMake. (vcpkg manifest mode is acknowledged as an equally valid alternative.)

**NFR-3** The project shall build cleanly with **no warnings** under `-Wall -Wextra -Wpedantic` for supported compilers.

### 3.2 Language Standard

**NFR-4** All C++ source code shall target **C++20** (`cmake_minimum_required` + `set(CMAKE_CXX_STANDARD 20)`). Compiler minimum versions: GCC ≥ 12, Clang ≥ 15, MSVC ≥ 19.34 (VS 2022 17.4).

### 3.3 Testing

**NFR-5** Unit tests shall use **GoogleTest** (≥ 1.14). Each public service class shall have an accompanying test suite. The test binary shall be invocable via `ctest` from the build directory.

**NFR-6** Code coverage reporting (e.g., `lcov`/`gcovr`) is desirable but out of scope for this iteration (see Section 5).

### 3.4 Containerization

**NFR-7** A **Dockerfile** shall be provided at the repository root using a **multi-stage build**:
  1. *Build stage*: a standard C++ toolchain image (e.g., `ubuntu:24.04` or `debian:bookworm`).
  2. *Final stage*: a **distroless** base image (e.g., `gcr.io/distroless/cc-debian12`) containing only the compiled binary and required shared libraries.

**NFR-8** The final container image shall contain no shell, package manager, or unnecessary tooling (distroless constraint).

**NFR-9** A `docker-compose.yml` (or `compose.yaml`) shall be provided to start the service, Envoy proxy, an OpenTelemetry collector, and a Prometheus scraper together for local development.

### 3.5 CI/CD

**NFR-10** A **GitHub Actions** workflow (`.github/workflows/ci.yml`) shall be provided that runs on every push and pull request to `main` and performs:
  1. CMake configure + build (Release and Debug configurations).
  2. `ctest` execution (all tests must pass).
  3. `clang-tidy` static analysis against the production source files (not generated proto files).

**NFR-11** The CI workflow shall cache the Conan 2 package store (`~/.conan2/p`) to reduce build times on repeated runs.

### 3.6 Performance Targets

**NFR-12** Under a single-core loopback benchmark the `Echo` RPC shall sustain ≥ 1,000 requests per second with p99 latency ≤ 10 ms. This target is indicative (for portfolio README claims) and does not need automated enforcement in CI for this iteration.

### 3.7 Thread-Safety

**NFR-13** The gRPC service implementation shall be safe for concurrent calls on multiple server threads (gRPC-default thread-pool model). No global mutable state shall be accessed without synchronization.

### 3.8 Documentation

**NFR-14** A `README.md` shall be present at the repository root with:
  - Project purpose and technology stack badges.
  - Prerequisites list (Docker, cmake, protoc, etc.).
  - Quickstart section: commands to build locally, run via Docker Compose, send a test RPC with `grpc_cli` or `grpcurl`, and `curl` the REST gateway.
  - Architecture diagram (ASCII or embedded image) showing the Envoy ↔ gRPC ↔ OTEL ↔ Prometheus topology.

---

## 4. Constraints & Assumptions

| # | Constraint / Assumption |
|---|-------------------------|
| C-1 | Target platform is **Linux x86-64**. ARM64 support is desirable but not tested in CI for this iteration. |
| C-2 | The repository is hosted on GitHub; GitHub Actions free tier is the only CI runtime assumed. |
| C-3 | No GPU, SIMD, or platform-specific intrinsics are required. |
| C-4 | The scaffold does not need to handle production traffic volumes; performance targets (NFR-12) exist only to substantiate portfolio claims. |
| C-5 | The Envoy version used in `docker-compose.yml` shall be pinned to a specific tag (not `latest`) to ensure reproducible builds. |
| C-6 | The OpenTelemetry collector in the local compose stack is for demonstration only; no production collector configuration is in scope. |

---

## 5. Out of Scope (This Iteration)

- **Authentication / Authorization** — No TLS mutual auth, JWT validation, or RBAC. Flagged as a high-priority future concern before any production deployment.
- **Persistence / Databases** — No database clients, ORMs, or migrations.
- **Service mesh configuration** — No Istio, Linkerd, or advanced Envoy policies beyond the basic JSON transcoding gateway.
- **Code coverage reporting** — Tooling (lcov/gcovr) is acknowledged but deferred.
- **Windows / macOS CI** — Only Linux CI runners are in scope.
- **gRPC server-side streaming implementation** — The streaming RPC variant shall be defined in the `.proto` file and documented, but a working implementation is optional for this iteration.
- **Helm chart / Kubernetes manifests** — Only Docker Compose for local orchestration.
- **Rate limiting, retries, circuit breaking** — Deferred to the service mesh layer.

---

## 6. Open Questions

| # | Question | Impact if unresolved |
|---|----------|----------------------|
| OQ-1 | What should the canonical service/domain name be (used in log `service` field, metric labels, package paths)? | Blocks proto package naming and label values. |
| OQ-2 | Is there a preference for the Prometheus client library (`prometheus-cpp` vs OpenTelemetry metrics SDK exporting to Prometheus)? Both are valid; choosing OTel metrics SDK would unify instrumentation under one SDK. | Affects NFR-2 (Conan 2 dependency list) and FR-17/FR-18 implementation approach. |
| OQ-3 | Should the `/metrics` HTTP server be a minimal embedded server (e.g., `cpp-httplib`) or should Envoy also proxy it? | Affects deploy topology and Envoy config scope. |
| OQ-4 | Is ARM64 / Apple Silicon local development a hard requirement (affects base image selection in NFR-7)? | Dockerfile `FROM` platform choices. |
| OQ-5 | What license should the scaffold carry? (Currently only `LICENSE` file exists with no content confirmed.) | README badge and third-party license compatibility check. |

---

## 7. Acceptance Criteria

The scaffold is considered complete for this iteration when **all** of the following are true:

| ID  | Criterion |
|-----|-----------|
| AC-1 | `cmake -B build && cmake --build build` completes with zero errors and zero warnings on a clean Ubuntu 24.04 environment. |
| AC-2 | `ctest --test-dir build` reports all tests passed. |
| AC-3 | `docker compose up` starts the full stack (service, Envoy, OTel collector, Prometheus) without manual intervention. |
| AC-4 | `grpcurl -plaintext localhost:50051 hello.HelloService/SayHello` returns a valid response. |
| AC-5 | `curl http://localhost:8080/v1/hello` returns a valid JSON response transcoded from the gRPC service. |
| AC-6 | `curl http://localhost:8080/healthz` returns HTTP 200 with `{"status":"ok"}`. |
| AC-7 | `curl http://localhost:9090/metrics` returns Prometheus text output containing `rpc_requests_total`, `rpc_request_duration_seconds`, and `rpc_active_connections`. |
| AC-8 | A trace appears in the OTel collector stdout (or Jaeger UI if included in compose) after the AC-4 call. |
| AC-9 | The GitHub Actions CI workflow passes (green) on a clean push to `main`. |
| AC-10 | `clang-tidy` reports zero issues on production source files in CI. |

---

*End of SRS — version 0.1.0*
