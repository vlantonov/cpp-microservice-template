# C++ Microservice Template

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)
[![CI](https://github.com/your-org/cpp-microservice-template/actions/workflows/ci.yml/badge.svg)](../../actions/workflows/ci.yml)

A production-ready C++20 gRPC microservice scaffold with structured logging,
Prometheus metrics, and OpenTelemetry distributed tracing.

---

## Tech Stack

| Component | Library / Image |
|-----------|----------------|
| RPC framework | [gRPC v1.65.5](https://github.com/grpc/grpc) (C++) |
| Protobuf | protobuf v27 (gRPC submodule) |
| REST transcoding | [Envoy v1.31](https://www.envoyproxy.io/) grpc_json_transcoder |
| Structured logging | [spdlog v1.14](https://github.com/gabime/spdlog) — JSON to stdout |
| Metrics | [prometheus-cpp v1.2.4](https://github.com/jupp0r/prometheus-cpp) + [cpp-httplib v0.16](https://github.com/yhirose/cpp-httplib) |
| Distributed tracing | [OpenTelemetry C++ SDK v1.16](https://github.com/open-telemetry/opentelemetry-cpp) + OTLP/gRPC |
| Unit tests | [GoogleTest v1.14](https://github.com/google/googletest) |
| Build system | CMake ≥ 3.21 + Ninja |

---

## Architecture

```
┌─────────┐  HTTP/JSON   ┌──────────────────┐  gRPC h2c :50051
│  curl / │─────────────▶│  Envoy :8080     │─────────────────────┐
│ browser │              │ grpc_json_       │                     │
└─────────┘              │ transcoder       │  ┌──────────────────▼──┐
                         │                  │  │   hello_server       │
                         │ GET /healthz     │  │  ┌──────────────┐   │
                         │  direct_response │  │  │ HelloService │   │
                         └──────────────────┘  │  └──────┬───────┘   │
                                               │         │            │
                                               │  ┌──────▼───────┐   │
  ┌──────────────────┐  OTLP/gRPC :4317        │  │   Logger     │   │
  │  OTel Collector  │◀────────────────────────│  │ (spdlog JSON)│   │
  └──────────────────┘                         │  └──────────────┘   │
                                               │  ┌──────────────┐   │
  ┌──────────────────┐  HTTP scrape :9090       │  │   Metrics    │   │
  │   Prometheus     │◀────────────────────────│  │ (prom-cpp)   │   │
  └──────────────────┘                         │  └──────────────┘   │
                                               │  ┌──────────────┐   │
                                               │  │   Tracer     │   │
                                               │  │ (OTel SDK)   │   │
                                               │  └──────────────┘   │
                                               └─────────────────────┘
                                               stdout → container logs
```

---

## Prerequisites

- CMake ≥ 3.21
- A C++20 compiler (`clang++` or `g++ ≥ 12`)
- Ninja
- Git
- Docker + Docker Compose (for the full stack)
- `libssl-dev`, `zlib1g-dev`, `pkg-config` (for gRPC)

---

## Quickstart

### Local build

```bash
# Configure (downloads all deps via FetchContent on first run — takes a while)
cmake -B build/debug -G Ninja -DCMAKE_BUILD_TYPE=Debug

# Build
cmake --build build/debug --parallel

# Run unit tests
ctest --test-dir build/debug --output-on-failure

# Start the server
./build/debug/src/hello_server
```

### Docker Compose (full stack)

```bash
# Build the proto descriptor set first (needed by Envoy)
cmake -B build/release -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build/release --target proto_descriptor_set

# Bring up all services
docker compose up --build
```

### Test the gRPC endpoint directly

```bash
# Using grpcurl
grpcurl -plaintext -d '{"message":"world"}' localhost:50051 hello.v1.HelloService/SayHello
```

### Test the REST endpoint via Envoy

```bash
# POST /v1/hello (Envoy transcodes to gRPC)
curl -s -X POST http://localhost:8080/v1/hello \
     -H 'Content-Type: application/json' \
     -d '{"message":"world"}' | jq .

# Health check (answered directly by Envoy)
curl -s http://localhost:8080/healthz
```

### Inspect Prometheus metrics

```bash
curl -s http://localhost:9090/metrics | grep rpc_requests_total
```

### Access Prometheus UI

Open [http://localhost:9091](http://localhost:9091) in a browser.

---

## Configuration

All configuration is via environment variables:

| Variable | Default | Description |
|----------|---------|-------------|
| `GRPC_PORT` | `50051` | gRPC listening port |
| `METRICS_PORT` | `9090` | Prometheus /metrics HTTP port |
| `LOG_LEVEL` | `info` | Log level: `debug`, `info`, `warn`, `error` |
| `OTEL_EXPORTER_OTLP_ENDPOINT` | `localhost:4317` | OTel collector gRPC endpoint |

---

## Project Structure

```
.
├── CMakeLists.txt           # Root: FetchContent + add_subdirectory
├── proto/
│   └── hello/v1/hello.proto # Service definition + HTTP annotations
├── src/
│   ├── main.cpp             # Server entry point
│   ├── hello_service.{hpp,cpp}
│   ├── logger.{hpp,cpp}
│   ├── metrics.{hpp,cpp}
│   └── tracer.{hpp,cpp}
├── tests/
│   ├── hello_service_test.cpp
│   ├── logger_test.cpp
│   ├── metrics_test.cpp
│   └── tracer_test.cpp
├── deploy/
│   ├── envoy/envoy.yaml
│   ├── otel-collector/otel-collector-config.yaml
│   └── prometheus/prometheus.yml
├── Dockerfile
└── docker-compose.yml
```

---

## License

MIT — see [LICENSE](LICENSE).
