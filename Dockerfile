# syntax=docker/dockerfile:1.7

# =============================================================================
# Multi-stage Dockerfile
#
# Stage 1 (toolchain):  ubuntu:26.04 - installs Conan 2 + C++ build tools.
# Stage 2 (conan-deps): cache-stable dependency resolution layer.
# Stage 3 (builder):    configure + compile with ccache.
# Stage 4 (verified):   run unit tests and collect runtime artifacts.
# Stage 5 (final):      gcr.io/distroless/cc-debian12 runtime image.
# =============================================================================

# ---------------------------------------------------------------------------
# Stage 1: Toolchain
# ---------------------------------------------------------------------------
FROM ubuntu:26.04 AS toolchain

RUN apt-get update && apt-get install -y --no-install-recommends \
        cmake \
        ninja-build \
        make \
        clang-18 \
        clang-tidy-18 \
        lld \
        ccache \
        python3-pip \
        git \
        ca-certificates \
        libssl-dev \
        pkg-config \
        zlib1g-dev \
    && rm -rf /var/lib/apt/lists/*

RUN pip3 install "conan>=2.0" --break-system-packages \
    && conan profile detect --force

WORKDIR /app

# ---------------------------------------------------------------------------
# Stage 2: Conan dependencies
# Keep this layer stable by copying only dependency-defining files first.
# ---------------------------------------------------------------------------
FROM toolchain AS conan-deps

COPY conanfile.py CMakeLists.txt CMakePresets.json ./
COPY conan ./conan

RUN conan install . \
        --profile:host conan/profiles/linux-clang18 \
        --profile:build conan/profiles/linux-clang18 \
        --build=missing \
        -s:h build_type=Release \
        -s:b build_type=Release

# ---------------------------------------------------------------------------
# Seed stage: replaced at build time with --build-context ccache-seed=.ccache
# so CI can inject the warm ccache exported from the previous run.
# ---------------------------------------------------------------------------
FROM scratch AS ccache-seed

# ---------------------------------------------------------------------------
# Stage 3: Build
# ---------------------------------------------------------------------------
FROM conan-deps AS builder

COPY proto ./proto
COPY src ./src
COPY tests ./tests

RUN cmake --preset conan-release \
        -DCMAKE_C_COMPILER_LAUNCHER=ccache \
        -DCMAKE_CXX_COMPILER_LAUNCHER=ccache

# Seed ccache from the GitLab-cached .ccache dir injected via --build-context.
RUN --mount=from=ccache-seed,target=/ccache-seed \
    cp -a /ccache-seed/. /root/.cache/ccache/ 2>/dev/null || true && \
    cmake --build --preset conan-release --parallel "$(nproc)" && \
    ccache -s

# ---------------------------------------------------------------------------
# Cache export: CI builds this target with --output to extract the warm
# ccache and persist it as a GitLab cache artifact for the next run.
# ---------------------------------------------------------------------------
FROM scratch AS ccache-export
COPY --from=builder /root/.cache/ccache /

# ---------------------------------------------------------------------------
# Stage 4: Verify and collect runtime assets
# ---------------------------------------------------------------------------
FROM builder AS verified

RUN run-clang-tidy-18 -p build/Release src/

RUN ctest --test-dir build/Release --output-on-failure --parallel "$(nproc)"

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
# Stage 5: Minimal runtime image
# ---------------------------------------------------------------------------
FROM gcr.io/distroless/cc-debian12 AS final

COPY --from=verified /runtime/hello_server /hello_server
COPY --from=verified /runtime/*.so* /usr/lib/

EXPOSE 50051
EXPOSE 9090

ENTRYPOINT ["/hello_server"]
