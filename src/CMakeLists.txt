# ---------------------------------------------------------------------------
# src/CMakeLists.txt
# Defines: hello_lib (STATIC), hello_server (EXECUTABLE)
# ---------------------------------------------------------------------------

# ---------------------------------------------------------------------------
# hello_lib — business logic + observability (§2.2)
# ---------------------------------------------------------------------------
add_library(hello_lib STATIC
    hello_service.cpp
    logger.cpp
    metrics.cpp
    tracer.cpp
)

target_compile_options(hello_lib PRIVATE -Wall -Wextra -Wpedantic)

# Public: src/ headers visible to consumers (hello_server, hello_tests).
target_include_directories(hello_lib
    PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}"
)

target_link_libraries(hello_lib
    PRIVATE
        prometheus-cpp::core
        httplib::httplib
    PUBLIC
        proto_gen
        spdlog::spdlog
        opentelemetry-cpp::opentelemetry-cpp
        gRPC::grpc++
)

# ---------------------------------------------------------------------------
# hello_server — entry point (§2.3)
# ---------------------------------------------------------------------------
add_executable(hello_server main.cpp)

target_compile_options(hello_server PRIVATE -Wall -Wextra -Wpedantic)

target_link_libraries(hello_server
    PRIVATE
        hello_lib
        gRPC::grpc++_reflection
)
