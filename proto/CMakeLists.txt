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
# PROTOC_EXECUTABLE, GRPC_CPP_PLUGIN_EXECUTABLE, and GOOGLEAPIS_PROTO_PATH
# are injected by conanfile.py's generate() into conan_toolchain.cmake.
# Using explicit paths avoids find_program() picking up the wrong system
# binary (e.g. system protoc v3.21 when Conan installs protobuf v27).

# ------------------------------------------------------------------
# Custom command: generate C++ stubs from vendored googleapis protos.
# These .pb.h files are transitively included by the generated hello.pb.h
# and must be compiled against the same protobuf version.
# ------------------------------------------------------------------
set(GOOGLEAPIS_GENERATED_SOURCES
    "${GENERATED_DIR}/google/api/annotations.pb.cc"
    "${GENERATED_DIR}/google/api/http.pb.cc"
)
set(GOOGLEAPIS_GENERATED_HEADERS
    "${GENERATED_DIR}/google/api/annotations.pb.h"
    "${GENERATED_DIR}/google/api/http.pb.h"
)

add_custom_command(
    OUTPUT
        ${GOOGLEAPIS_GENERATED_SOURCES}
        ${GOOGLEAPIS_GENERATED_HEADERS}
    COMMAND "${PROTOC_EXECUTABLE}"
        "--proto_path=${GOOGLEAPIS_PROTO_PATH}"
        "--cpp_out=${GENERATED_DIR}"
        "google/api/annotations.proto"
        "google/api/http.proto"
    DEPENDS
        "${GOOGLEAPIS_PROTO_PATH}/google/api/annotations.proto"
        "${GOOGLEAPIS_PROTO_PATH}/google/api/http.proto"
        "${PROTOC_EXECUTABLE}"
    COMMENT "Generating googleapis C++ stubs"
    VERBATIM
)

add_custom_command(
    OUTPUT
        ${GENERATED_SOURCES}
        ${GENERATED_HEADERS}
    COMMAND "${PROTOC_EXECUTABLE}"
        "--proto_path=${GOOGLEAPIS_PROTO_PATH}"
        "--proto_path=${CMAKE_SOURCE_DIR}/proto"
        "--cpp_out=${GENERATED_DIR}"
        "--grpc_out=${GENERATED_DIR}"
        "--plugin=protoc-gen-grpc=${GRPC_CPP_PLUGIN_EXECUTABLE}"
        "hello/v1/hello.proto"
    DEPENDS
        ${PROTO_SOURCES}
        ${GOOGLEAPIS_GENERATED_HEADERS}
        "${PROTOC_EXECUTABLE}"
        "${GRPC_CPP_PLUGIN_EXECUTABLE}"
    COMMENT "Generating protobuf + gRPC C++ stubs"
    VERBATIM
)

# ------------------------------------------------------------------
# Custom command: generate binary descriptor set for Envoy
# ------------------------------------------------------------------
add_custom_command(
    OUTPUT "${CMAKE_BINARY_DIR}/proto.pb"
    COMMAND "$<TARGET_FILE:protobuf::protoc>"
        "--proto_path=${GOOGLEAPIS_PROTO_PATH}"
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
    ${GOOGLEAPIS_GENERATED_SOURCES}
    ${GOOGLEAPIS_GENERATED_HEADERS}
    ${GENERATED_SOURCES}
    ${GENERATED_HEADERS}
)

# Suppress warnings on generated code
target_compile_options(proto_gen PRIVATE -w)

# Expose the generated directory so consumers can include google/api/*.pb.h
# and hello/v1/*.pb.h without knowing the build-tree layout.
target_include_directories(proto_gen PUBLIC "${GENERATED_DIR}")

# Conan-provided protobuf/gRPC libraries are required to compile the
# generated .pb.cc stubs and to propagate includes to consumers.
target_link_libraries(proto_gen
    PUBLIC
        protobuf::libprotobuf
        gRPC::grpc++
)

target_include_directories(proto_gen
    PUBLIC "${GENERATED_DIR}"
)

target_link_libraries(proto_gen
    PUBLIC
        protobuf::libprotobuf
        gRPC::grpc++
)
