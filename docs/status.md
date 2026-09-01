# Project Status

| Field              | Value                          |
|--------------------|--------------------------------|
| SDLC Stage         | Released (active maintenance)  |
| Version            | v0.1.0 (patch 2 applied)       |
| Release Date       | 2026-07-14                     |
| Last Updated       | 2026-07-17                     |
| QA Sign-off        | Approved — zero blocking defects |

---

## Post-Release Patches (2026-09-01)

### Patch 5 — CI build-time optimizations (`77dd8a9`, `82cb1a8`, `0d250fe`)

QA: PASS (static/logical review, all checks green). Three `semver(patch)` commits on `dependabot/docker/ubuntu-26.04`:

- **`77dd8a9`** — Persist ccache across CI runs. Added `ccache-seed` (`FROM scratch`, overridden by `--build-context ccache-seed=.ccache`) and `ccache-export` stages to the `Dockerfile`; `.gitlab-ci.yml` seeds the warm cache, runs an export build, and rotates `.ccache`/`.buildx-cache`. `ccache -s` stats printed for observability.
- **`82cb1a8`** — Cache the Conan package store via `--mount=type=cache,target=/root/.conan2/p` on the `conan install` step so gRPC/protobuf/abseil aren't rebuilt from source on layer invalidation.
- **`0d250fe`** — Removed `run-clang-tidy-18` from the `verified` stage; added a parallel `clang-tidy` job (same `docker` stage, `policy: pull`) that builds the `builder` target and lints via `docker run`, taking linting off the image-build critical path.

**Files changed:** `Dockerfile`, `.gitlab-ci.yml`

**Known trade-off:** on the first-ever pipeline (cold cache) `docker-build` and `clang-tidy` compile independently; converges from the second run onward.

**Files changed:** `Dockerfile`, `.gitlab-ci.yml`

---

### Patch 4 — `01fcb1e` — `Dockerfile` — fix versioned clang-tidy binary

**Problem:** CI stage `verified` called `run-clang-tidy` which is not installed; `clang-tidy-18` provides `run-clang-tidy-18` only (exit code 127).

**Fix:** Changed `RUN run-clang-tidy …` to `RUN run-clang-tidy-18 …` in the `verified` stage.

**Files changed:** `Dockerfile`

**Note:** `docs/design/microservice-design.md` lines 162 and 641 still reference the unversioned `run-clang-tidy` — doc-only, no functional impact.

---

## Post-Release Patches (2026-07-17, continued)

### Patch 3 — `.gitlab-ci.yml` — GitLab CI pipeline

**Change:** Added `.gitlab-ci.yml` mirroring `.github/workflows/ci.yml`.
Stages: `build-test` (Debug + Release matrix jobs with Conan 2 cache, CTest, and clang-tidy) and `docker` (Dockerfile smoke-build, no push). Triggers on pushes to `main` and merge-request events targeting `main`.

**Files changed:** `.gitlab-ci.yml`

---

## Post-Release Patches (2026-07-17)

Two `semver(patch)` commits applied to `main` after the initial release. Version tag remains `v0.1.0`; no public API or behavioural contract changes.

### Patch 1 — `4b298ec` — `src/metrics.cpp` gauge pre-initialisation

**Problem:** `prometheus-cpp` only serialises metric families that have at least one registered child instance. `rpc_active_connections` had no child until `SetActiveConnections()` was first called, causing the gauge to be absent from the `/metrics` HTTP response and failing the CI test `MetricsHttpTest.EndpointReturnsPrometheusTextFormat`.

**Fix:** `Metrics::Metrics()` constructor now calls `active_connections.Add({service}).Set(0.0)` so the gauge child exists from startup. `Family<Gauge>::Add()` is idempotent for identical label sets, so subsequent `SetActiveConnections()` calls are unaffected.

**Files changed:** `src/metrics.cpp`

---

### Patch 2 — `8757084` — Agent instruction updates

**Problem:** The blanket `FetchContent` prohibition in agent definitions was overly prescriptive for a Conan 2 project, and the hardcoded profile path `conan/profiles/linux-clang18` made maintenance instructions brittle.

**Fix:**
- `cpp-developer.agent.md`: replaced hard FetchContent ban with neutral description of Conan-generated find-modules.
- `maintenance-engineer.agent.md`: removed FetchContent parenthetical; generalized profile reference to "used Conan profiles".
- `system-architect.agent.md`: removed "Do not specify FetchContent" clause; retained restriction on bare system-installed library paths.

**Files changed:** `.github/agents/cpp-developer.agent.md`, `.github/agents/maintenance-engineer.agent.md`, `.github/agents/system-architect.agent.md`

---

## What Shipped

The following features from SRS §2.1–2.5 are included in this release:

### §2.1 gRPC Service
- **FR-1** `hello.HelloService` unary `SayHello` RPC defined in `proto/hello/v1/hello.proto`
- **FR-2** gRPC server port configurable via `GRPC_PORT` environment variable (default: `50051`)
- **FR-3** gRPC Health Checking Protocol (`grpc.health.v1.Health`) implemented
- **FR-4** Well-formed gRPC error status returned on invalid / unprocessable requests

### §2.2 REST Gateway
- **FR-5** HTTP/1.1 JSON REST interface transcoding to/from the gRPC service
- **FR-6** Envoy proxy with `envoy.filters.http.grpc_json_transcoder` filter
- **FR-7** Versioned Envoy config at `deploy/envoy/envoy.yaml`; REST API exposed on port `8080`
- **FR-8** `/healthz` endpoint returning HTTP 200 `{"status":"ok"}`

### §2.3 Structured Logging
- **FR-9** Newline-delimited JSON log records with `timestamp`, `severity`, `service`, `trace_id`, `message`
- **FR-10** spdlog ≥ 1.12; log level configurable via `LOG_LEVEL` environment variable
- **FR-11** All log output written to `stdout`

### §2.4 Distributed Tracing
- **FR-12** OpenTelemetry C++ SDK ≥ 1.14 integrated
- **FR-13** Trace span created per inbound RPC call (records method, status code, error message)
- **FR-14** W3C `traceparent`/`tracestate` context extracted from incoming gRPC metadata
- **FR-15** Traces exported via OTLP/gRPC (default: `localhost:4317`; configurable via `OTEL_EXPORTER_OTLP_ENDPOINT`)
- **FR-16** Graceful degradation when no collector is reachable (warning logged, request handling unaffected)

### §2.5 Prometheus Metrics
- **FR-17** `/metrics` endpoint on port `9090` (configurable via `METRICS_PORT`) in Prometheus text format
- **FR-18** Metrics: `rpc_requests_total` (counter), `rpc_request_duration_seconds` (histogram), `rpc_active_connections` (gauge)

---

## SRS Requirements Met

All **18** functional requirements (FR-1 through FR-18) verified by the QA Engineer agent with zero blocking defects.

Non-functional requirements covered:

| NFR    | Description                                                    | Status    |
|--------|----------------------------------------------------------------|-----------|
| NFR-1  | CMake ≥ 3.21                                                   | Verified  |
| NFR-2  | Conan 2 dependency management                                  | Verified  |
| NFR-3  | Clean build under `-Wall -Wextra -Wpedantic`                   | Verified  |
| NFR-4  | C++20 standard                                                 | Verified  |
| NFR-5  | GoogleTest ≥ 1.14; `ctest` invocable                           | Verified  |
| NFR-7  | Multi-stage Dockerfile (ubuntu:24.04 builder → distroless)     | Verified  |
| NFR-8  | Distroless final image (no shell or package manager)           | Verified  |
| NFR-9  | `docker-compose.yml` starts full local stack                   | Verified  |
| NFR-10 | GitHub Actions CI workflow (push + PR, Debug + Release, ctest, clang-tidy) | Verified  |
| NFR-11 | Conan 2 package cache in CI                                    | Verified  |
| NFR-14 | `README.md` with purpose, prerequisites, quickstart, architecture | Verified  |

---

## Known Limitations (SRS §5 — Out of Scope)

- **Authentication / Authorization** — No TLS mutual auth, JWT validation, or RBAC. High-priority concern before any production deployment.
- **Persistence / Databases** — No database clients, ORMs, or migrations.
- **Service mesh configuration** — No Istio, Linkerd, or advanced Envoy policies beyond the basic JSON transcoding gateway.
- **Code coverage reporting** — lcov/gcovr tooling deferred.
- **Windows / macOS CI** — Only Linux (ubuntu-24.04) runners in scope.
- **gRPC server-side streaming** — Defined in `.proto` and documented; working implementation optional for this iteration.
- **Helm chart / Kubernetes manifests** — Docker Compose only for local orchestration.
- **Rate limiting, retries, circuit breaking** — Deferred to service mesh layer.

---

## Next Iteration Suggestions

| Priority | Item                                                                 |
|----------|----------------------------------------------------------------------|
| High     | **TLS / mTLS** — mutual TLS between Envoy and gRPC server            |
| High     | **Authentication** — JWT validation at the Envoy layer or service level |
| Medium   | **Helm chart** — Kubernetes-ready deployment manifests               |
| Medium   | **ARM64 CI** — add `linux/arm64` to the Docker build matrix and CI runners |
| Low      | **Code coverage** — integrate lcov/gcovr and report to CI artifact   |
| Low      | **gRPC streaming** — implement the optional server-side streaming RPC |
