# =============================================================================
# Multi-stage Dockerfile
#
# Stage 1 (builder): ubuntu:24.04 — installs Conan 2, fetches deps, builds.
# Stage 2 (final):   gcr.io/distroless/cc-debian12 — minimal runtime image.
# =============================================================================

# ---------------------------------------------------------------------------
# Stage 1: Build
# ---------------------------------------------------------------------------
FROM ubuntu:24.04 AS builder

# Install build toolchain + Python (required for Conan 2).
RUN apt-get update && apt-get install -y --no-install-recommends \
        cmake \
        ninja-build \
        make \
        clang \
        python3-pip \
        git \
        ca-certificates \
        libssl-dev \
        pkg-config \
        zlib1g-dev \
    && rm -rf /var/lib/apt/lists/*

# Install Conan 2 and generate a default profile.
RUN pip3 install "conan>=2.0" --break-system-packages \
    && conan profile detect

WORKDIR /app

# Copy source tree
COPY . .

# Install all dependencies via Conan (builds from source if no binary
# package is available for this platform).
COPY conan/profiles/linux-clang18 /root/.conan2/profiles/default
RUN conan install . \
        --profile:host conan/profiles/linux-clang18 \
        --profile:build conan/profiles/linux-clang18 \
        --build=missing \
        -s:h build_type=Release \
        -s:b build_type=Release

# Configure and build in Release mode using the Conan-generated preset.
RUN cmake --preset conan-release \
    && cmake --build --preset conan-release --parallel "$(nproc)"

# Collect shared-library dependencies so they can be copied to the final stage.
RUN mkdir /runtime && \
    cp /app/build/Release/src/hello_server /runtime/hello_server && \
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
