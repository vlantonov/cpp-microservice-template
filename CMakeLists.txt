cmake_minimum_required(VERSION 3.27)
project(cpp_microservice_template VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(BUILD_TESTING "Build tests" ON)

find_package(Protobuf REQUIRED)
find_package(gRPC REQUIRED)
find_package(spdlog REQUIRED)
find_package(nlohmann_json REQUIRED)
find_package(prometheus-cpp REQUIRED)
find_package(opentelemetry-cpp CONFIG REQUIRED)
find_package(httplib REQUIRED)

set(PROTO_FILE ${CMAKE_CURRENT_SOURCE_DIR}/proto/greeter/v1/greeter.proto)
set(PROTO_BINARY_DIR ${CMAKE_CURRENT_BINARY_DIR}/generated)
file(MAKE_DIRECTORY ${PROTO_BINARY_DIR})

set(PROTO_SRCS ${PROTO_BINARY_DIR}/greeter/v1/greeter.pb.cc)
set(PROTO_HDRS ${PROTO_BINARY_DIR}/greeter/v1/greeter.pb.h)
set(GRPC_SRCS ${PROTO_BINARY_DIR}/greeter/v1/greeter.grpc.pb.cc)
set(GRPC_HDRS ${PROTO_BINARY_DIR}/greeter/v1/greeter.grpc.pb.h)

add_custom_command(
  OUTPUT ${PROTO_SRCS} ${PROTO_HDRS} ${GRPC_SRCS} ${GRPC_HDRS}
  COMMAND protobuf::protoc
  ARGS --proto_path=${CMAKE_CURRENT_SOURCE_DIR}/proto
       --cpp_out=${PROTO_BINARY_DIR}
       --grpc_out=${PROTO_BINARY_DIR}
       --plugin=protoc-gen-grpc=$<TARGET_FILE:gRPC::grpc_cpp_plugin>
       ${PROTO_FILE}
  DEPENDS ${PROTO_FILE}
  VERBATIM)

add_library(proto_generated STATIC
  ${PROTO_SRCS}
  ${GRPC_SRCS})

target_include_directories(proto_generated
  PUBLIC
    ${PROTO_BINARY_DIR})

target_link_libraries(proto_generated
  PUBLIC
    protobuf::libprotobuf
    gRPC::grpc++)

add_library(core_service STATIC
  src/core/greeter_service.cpp)

target_include_directories(core_service
  PUBLIC
    include)

target_link_libraries(core_service
  PUBLIC
    nlohmann_json::nlohmann_json)

add_library(observability STATIC
  src/observability/logger.cpp
  src/observability/metrics.cpp
  src/observability/tracing.cpp)

target_include_directories(observability
  PUBLIC
    include)

target_link_libraries(observability
  PUBLIC
    spdlog::spdlog
    prometheus-cpp::core
    prometheus-cpp::pull
    opentelemetry-cpp::trace
    opentelemetry-cpp::otlp_http_exporter)

add_library(transport_grpc STATIC
  src/transport/grpc/greeter_server.cpp)

target_include_directories(transport_grpc
  PUBLIC
    include)

target_link_libraries(transport_grpc
  PUBLIC
    core_service
    observability
    proto_generated
    gRPC::grpc++)

add_library(transport_rest STATIC
  src/transport/rest/rest_server.cpp)

target_include_directories(transport_rest
  PUBLIC
    include)

target_link_libraries(transport_rest
  PUBLIC
    core_service
    observability
    httplib::httplib
    nlohmann_json::nlohmann_json)

add_executable(microservice
  src/main.cpp)

target_include_directories(microservice
  PRIVATE
    include)

target_link_libraries(microservice
  PRIVATE
    transport_grpc
    transport_rest
    observability)

if(BUILD_TESTING)
  enable_testing()
endif()
