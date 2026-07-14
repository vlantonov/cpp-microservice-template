cmake_minimum_required(VERSION 3.21)

# ---------------------------------------------------------------------------
# proto/CMakeLists.txt
# Compiles hello.proto into C++/gRPC stubs (proto_gen STATIC library) and
# generates proto.pb descriptor set for Envoy grpc_json_transcoder.
# ---------------------------------------------------------------------------

set(GENERATED_DIR "${CMAKE_BINARY_DIR}/generated")
file(MAKE_DIRECTORY "${GENERATED_DIR}")

set(PROTO_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/hello/v1/hello.proto"
)

set(GENERATED_SOURCES
    "${GENERATED_DIR}/hello/v1/hello.pb.cc"
    "${GENERATED_DIR}/hello/v1/hello.grpc.pb.cc"
)

set(GENERATED_HEADERS
    "${GENERATED_DIR}/hello/v1/hello.pb.h"
    "${GENERATED_DIR}/hello/v1/hello.grpc.pb.h"
)

# ------------------------------------------------------------------
# Custom command: generate C++ + gRPC stubs
# ------------------------------------------------------------------
add_custom_command(
    OUTPUT
        ${GENERATED_SOURCES}
        ${GENERATED_HEADERS}
    COMMAND "$<TARGET_FILE:protobuf::protoc>"
        "--proto_path=${googleapis_SOURCE_DIR}"
        "--proto_path=${CMAKE_SOURCE_DIR}/proto"
        "--cpp_out=${GENERATED_DIR}"
        "--grpc_out=${GENERATED_DIR}"
        "--plugin=protoc-gen-grpc=$<TARGET_FILE:grpc_cpp_plugin>"
        "hello/v1/hello.proto"
    DEPENDS
        ${PROTO_SOURCES}
        protobuf::protoc
        grpc_cpp_plugin
    COMMENT "Generating protobuf + gRPC C++ stubs"
    VERBATIM
)

# ------------------------------------------------------------------
# Custom command: generate binary descriptor set for Envoy
# ------------------------------------------------------------------
add_custom_command(
    OUTPUT "${CMAKE_BINARY_DIR}/proto.pb"
    COMMAND "$<TARGET_FILE:protobuf::protoc>"
        "--proto_path=${googleapis_SOURCE_DIR}"
        "--proto_path=${CMAKE_SOURCE_DIR}/proto"
        "--descriptor_set_out=${CMAKE_BINARY_DIR}/proto.pb"
        "--include_imports"
        "hello/v1/hello.proto"
    DEPENDS
        ${PROTO_SOURCES}
        protobuf::protoc
    COMMENT "Generating proto descriptor set for Envoy"
    VERBATIM
)

add_custom_target(proto_descriptor_set ALL
    DEPENDS "${CMAKE_BINARY_DIR}/proto.pb"
)

# ------------------------------------------------------------------
# proto_gen STATIC library
# ------------------------------------------------------------------
add_library(proto_gen STATIC
    ${GENERATED_SOURCES}
    ${GENERATED_HEADERS}
)

# Suppress warnings on generated code
target_compile_options(proto_gen PRIVATE -w)

target_include_directories(proto_gen
    PUBLIC "${GENERATED_DIR}"
)

target_link_libraries(proto_gen
    PUBLIC
        protobuf::libprotobuf
        gRPC::grpc++
)
