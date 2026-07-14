# =============================================================================
# Multi-stage Dockerfile
#
# Stage 1 (builder): ubuntu:24.04 — builds the binary with CMake/FetchContent.
# Stage 2 (final):   gcr.io/distroless/cc-debian12 — minimal runtime image.
# =============================================================================

# ---------------------------------------------------------------------------
# Stage 1: Build
# ---------------------------------------------------------------------------
FROM ubuntu:24.04 AS builder

# Install only what the build system needs (no system gRPC/protobuf).
RUN apt-get update && apt-get install -y --no-install-recommends \
        cmake \
        ninja-build \
        clang \
        git \
        ca-certificates \
        libssl-dev \
        pkg-config \
        zlib1g-dev \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

# Copy source tree
COPY . .

# Configure and build in Release mode.
# FetchContent downloads go into .fetchcontent (gitignored, docker-cached layer).
RUN cmake -B build/release \
        -G Ninja \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_CXX_COMPILER=clang++ \
        -DFETCHCONTENT_BASE_DIR=/app/.fetchcontent \
    && cmake --build build/release --parallel "$(nproc)"

# Collect shared-library dependencies so they can be copied to the final stage.
RUN mkdir /runtime && \
    cp /app/build/release/src/hello_server /runtime/hello_server && \
    ldd /runtime/hello_server 2>/dev/null \
        | grep "=> /" \
        | awk '{print $3}' \
        | sort -u \
        | while IFS= read -r lib; do \
            cp --no-preserve=mode,ownership "$lib" /runtime/ || true; \
          done

# ---------------------------------------------------------------------------
# Stage 2: Minimal runtime image
# ---------------------------------------------------------------------------
FROM gcr.io/distroless/cc-debian12 AS final

# Copy binary
COPY --from=builder /runtime/hello_server /hello_server

# Copy any shared libraries not already in distroless/cc-debian12.
# (distroless/cc already ships libc6, libstdc++, libgcc-s1)
COPY --from=builder /runtime/*.so*  /usr/lib/

# Service ports
EXPOSE 50051
EXPOSE 9090

ENTRYPOINT ["/hello_server"]
