# C++ Microservice Template

Production-oriented C++ scaffold with:
- gRPC API
- in-process REST gateway
- structured JSON logging
- OpenTelemetry traces (OTLP HTTP)
- Prometheus metrics endpoint
- Kubernetes deployment baseline

## Quickstart (local)

1. Install Conan 2, CMake 3.27+, Ninja, and a C++20 compiler.
2. Install dependencies:

```bash
conan profile detect --force
conan install . --output-folder=build --build=missing -s build_type=Debug
```

3. Configure and build:

```bash
cmake -S . -B build/debug -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE=build/build/Debug/generators/conan_toolchain.cmake \
  -DCMAKE_BUILD_TYPE=Debug
cmake --build build/debug
```

4. Run the service:

```bash
./build/debug/microservice
```

Endpoints:
- REST health: `GET /healthz` on `:8080`
- REST hello: `POST /api/v1/hello` on `:8080`
- gRPC: `:50051`
- Prometheus metrics: `:9464/metrics`

## Local Observability Stack

```bash
cd deploy/observability
docker compose up -d
```

- Grafana: http://localhost:3000 (admin/admin)
- Prometheus: http://localhost:9090
- Tempo: http://localhost:3200

## Kubernetes Baseline

```bash
kubectl apply -f deploy/k8s/
```

## Current Scope

This initial implementation is a scaffold slice focused on transport and observability wiring. Add business modules, tests, authn/authz, and CI policy gates before production rollout.
