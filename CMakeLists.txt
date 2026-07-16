cmake_minimum_required(VERSION 3.23)
project(cpp_microservice_template VERSION 0.1.0 LANGUAGES CXX)

# ---------------------------------------------------------------------------
# Global CMake settings (§3.3 of design doc)
# ---------------------------------------------------------------------------
set(CMAKE_CXX_STANDARD          20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS        OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ---------------------------------------------------------------------------
# Dependencies — provided by Conan 2 via conanfile.py.
# Workflow:
#   conan install . --build=missing -s build_type=<Debug|Release> \
#       -s compiler=clang -s compiler.version=18 \
#       -s compiler.libcxx=libstdc++11 -s compiler.cppstd=20
#   cmake --preset conan-<debug|release>
# ---------------------------------------------------------------------------
find_package(gRPC             REQUIRED)
find_package(Protobuf         REQUIRED)
find_package(spdlog           REQUIRED)
find_package(prometheus-cpp   REQUIRED)
find_package(opentelemetry-cpp REQUIRED)
find_package(httplib          REQUIRED)
find_package(GTest            REQUIRED)

# ---------------------------------------------------------------------------
# Subdirectories
# ---------------------------------------------------------------------------
enable_testing()

add_subdirectory(proto)
add_subdirectory(src)
add_subdirectory(tests)
