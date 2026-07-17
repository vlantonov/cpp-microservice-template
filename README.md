# C++ Microservice Template

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)
[![CI](https://github.com/vlantonov/cpp-microservice-template/actions/workflows/ci.yml/badge.svg)](https://github.com/vlantonov/cpp-microservice-template/actions/workflows/ci.yml)

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
                         │                  │  │   hello_server      │
                         │ GET /healthz     │  │  ┌──────────────┐   │
                         │  direct_response │  │  │ HelloService │   │
                         └──────────────────┘  │  └──────┬───────┘   │
                                               │         │           │
                                               │  ┌──────▼───────┐   │
  ┌──────────────────┐  OTLP/gRPC :4317        │  │   Logger     │   │
  │  OTel Collector  │◀────────────────────────│  │ (spdlog JSON)│   │
  └──────────────────┘                         │  └──────────────┘   │
                                               │  ┌──────────────┐   │
  ┌──────────────────┐  HTTP scrape :9090      │  │   Metrics    │   │
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

- CMake ≥ 3.23
- Conan 2 (`pip install "conan>=2.0"`)
- A C++20 compiler (`clang++` or `g++ ≥ 12`)
- Ninja
- Git
- Docker + Docker Compose (for the full stack)
- `libssl-dev`, `zlib1g-dev`, `pkg-config` (needed when Conan builds some deps from source)

---

## Quickstart

### Local build

```bash
# Install Conan dependencies (first run downloads/builds — takes a while)
conan install . --profile:host conan/profiles/linux-clang18 \
              --profile:build conan/profiles/linux-clang18 \
              --build=missing -s:h build_type=Debug -s:b build_type=Debug

# Configure
cmake --preset conan-debug

# Build
cmake --build --preset conan-debug --parallel

# Run unit tests
ctest --test-dir build/Debug --output-on-failure

# Start the server
./build/Debug/src/hello_server
```

### Docker Compose (full stack)

```bash
# Install Conan dependencies and configure for Release
conan install . --profile:host conan/profiles/linux-clang18 \
              --profile:build conan/profiles/linux-clang18 \
              --build=missing -s:h build_type=Release -s:b build_type=Release
cmake --preset conan-release

# Build the proto descriptor set first (needed by Envoy)
cmake --build --preset conan-release --target proto_descriptor_set

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
├── CMakeLists.txt           # Root: find_package() + add_subdirectory
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
