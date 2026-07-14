cmake_minimum_required(VERSION 3.21)
project(cpp_microservice_template VERSION 0.1.0 LANGUAGES CXX)

# ---------------------------------------------------------------------------
# Global CMake settings (§3.3 of design doc)
# ---------------------------------------------------------------------------
set(CMAKE_CXX_STANDARD          20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS        OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Stable FetchContent cache that survives binary-dir wipes (§3.3)
set(FETCHCONTENT_BASE_DIR "${CMAKE_SOURCE_DIR}/.fetchcontent"
    CACHE PATH "FetchContent download/build cache location")

include(FetchContent)
set(FETCHCONTENT_QUIET OFF)   # show fetch progress in CI (§12)
# Skip git update step when content is already present in the cache dir.
# This prevents CI cache-restore failures where the .git directory is
# incomplete after actions/cache restoration.
set(FETCHCONTENT_UPDATES_DISCONNECTED ON)

# ---------------------------------------------------------------------------
# Dependency option overrides — MUST be set before FetchContent_MakeAvailable
# ---------------------------------------------------------------------------

# gRPC (§7.2)
set(gRPC_BUILD_TESTS              OFF CACHE BOOL "" FORCE)
set(gRPC_BUILD_GRPC_CPP_PLUGIN    ON  CACHE BOOL "" FORCE)
set(gRPC_BUILD_GRPC_PYTHON_PLUGIN OFF CACHE BOOL "" FORCE)
set(gRPC_BUILD_GRPC_RUBY_PLUGIN   OFF CACHE BOOL "" FORCE)
set(gRPC_BUILD_GRPC_PHP_PLUGIN    OFF CACHE BOOL "" FORCE)
set(gRPC_BUILD_GRPC_NODE_PLUGIN   OFF CACHE BOOL "" FORCE)
set(gRPC_BUILD_GRPC_CSHARP_PLUGIN OFF CACHE BOOL "" FORCE)
set(gRPC_BUILD_CSHARP_EXT         OFF CACHE BOOL "" FORCE)

# prometheus-cpp (§7.2): use core-only; expose via cpp-httplib
set(ENABLE_PULL    OFF CACHE BOOL "" FORCE)
set(ENABLE_PUSH    OFF CACHE BOOL "" FORCE)
set(ENABLE_TESTING OFF CACHE BOOL "" FORCE)  # prometheus-cpp own tests

# opentelemetry-cpp (§7.2)
set(WITH_OTLP_GRPC         ON  CACHE BOOL "" FORCE)
set(WITH_OTLP_HTTP         OFF CACHE BOOL "" FORCE)
set(WITH_ABSEIL            ON  CACHE BOOL "" FORCE)
set(BUILD_TESTING          OFF CACHE BOOL "" FORCE)
set(OPENTELEMETRY_INSTALL  OFF CACHE BOOL "" FORCE)

# ---------------------------------------------------------------------------
# FetchContent declarations  (§7.1 — all versions pinned, no branch names)
# ---------------------------------------------------------------------------

# gRPC v1.65.5 — brings in protobuf v27.x as a submodule
FetchContent_Declare(
    grpc
    GIT_REPOSITORY https://github.com/grpc/grpc
    GIT_TAG        v1.65.5
    GIT_SUBMODULES_RECURSE ON
)

# googleapis — proto files only (DOWNLOAD_ONLY: no CMakeLists.txt to build)
# Pinned SHA referenced by gRPC v1.65.5 submodule for proto compatibility.
FetchContent_Declare(
    googleapis
    GIT_REPOSITORY https://github.com/googleapis/googleapis
    GIT_TAG        ade23d0bca40f6e9f4fb23a51a39cf46e0e9e1d2
    # GIT_SHALLOW is intentionally omitted: shallow clones cannot fetch
    # arbitrary commit SHAs from GitHub (only advertised tags/branches work).
    DOWNLOAD_ONLY  YES
)

# spdlog
FetchContent_Declare(
    spdlog
    GIT_REPOSITORY https://github.com/gabime/spdlog
    GIT_TAG        v1.14.1
    GIT_SHALLOW    TRUE
)

# prometheus-cpp
FetchContent_Declare(
    prometheus-cpp
    GIT_REPOSITORY https://github.com/jupp0r/prometheus-cpp
    GIT_TAG        v1.2.4
    GIT_SHALLOW    TRUE
)

# opentelemetry-cpp
FetchContent_Declare(
    opentelemetry-cpp
    GIT_REPOSITORY https://github.com/open-telemetry/opentelemetry-cpp
    GIT_TAG        v1.16.1
    GIT_SHALLOW    TRUE
)

# cpp-httplib (header-only)
FetchContent_Declare(
    cpp-httplib
    GIT_REPOSITORY https://github.com/yhirose/cpp-httplib
    GIT_TAG        v0.16.3
    GIT_SHALLOW    TRUE
)

# GoogleTest
FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest
    GIT_TAG        v1.14.0
    GIT_SHALLOW    TRUE
)

# ---------------------------------------------------------------------------
# Make all dependencies available
# googleapis uses DOWNLOAD_ONLY YES so no add_subdirectory() is attempted.
# ---------------------------------------------------------------------------
FetchContent_MakeAvailable(
    grpc
    googleapis
    spdlog
    prometheus-cpp
    opentelemetry-cpp
    cpp-httplib
    googletest
)

# ---------------------------------------------------------------------------
# Subdirectories
# ---------------------------------------------------------------------------
enable_testing()

add_subdirectory(proto)
add_subdirectory(src)
add_subdirectory(tests)
