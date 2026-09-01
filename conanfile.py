import os

from conan import ConanFile
from conan.tools.cmake import cmake_layout, CMakeToolchain, CMakeDeps


class CppMicroserviceTemplateConan(ConanFile):
    name = "cpp_microservice_template"
    settings = "os", "compiler", "build_type", "arch"

    # ------------------------------------------------------------------
    # Dependencies
    # ------------------------------------------------------------------
    def requirements(self):
        # gRPC v1.69.0 — brings in protobuf and abseil as transitive deps.
        # Nearest stable CCI version to the original v1.65.5 pin.
        self.requires("grpc/1.69.0")

        # googleapis is NOT declared as a Conan dep because the CCI recipe
        # (cci.20230501) requires protobuf/3.21.12 which conflicts with
        # grpc/1.69.0's protobuf/5.27.0.  The two .proto source files
        # required by hello.proto are vendored at
        # proto/third_party/googleapis/ instead.

        self.requires("spdlog/1.14.1")
        self.requires("prometheus-cpp/1.2.4")

        # opentelemetry-cpp v1.21.0 — nearest stable CCI version to the
        # original v1.16.1 pin that is still maintained.
        self.requires("opentelemetry-cpp/1.21.0")

        self.requires("cpp-httplib/0.16.3")

        # GoogleTest — test-only; no link into production libraries.
        self.requires("gtest/1.14.0")

    # ------------------------------------------------------------------
    # Package options
    # ------------------------------------------------------------------
    def configure(self):
        # opentelemetry-cpp: enable OTLP/gRPC exporter only.
        # All HTTP-based exporters are disabled to avoid the libcurl
        # transitive dependency (whose CCI recipe requires Conan >= 2.21).
        self.options["opentelemetry-cpp"].with_otlp_grpc = True
        self.options["opentelemetry-cpp"].with_otlp_http = False
        self.options["opentelemetry-cpp"].with_otlp_http_compression = False
        self.options["opentelemetry-cpp"].with_zipkin = False
        self.options["opentelemetry-cpp"].with_jaeger = False
        self.options["opentelemetry-cpp"].with_elasticsearch = False
        self.options["opentelemetry-cpp"].with_abseil = True

        # prometheus-cpp: core library only (no pull/push HTTP servers).
        self.options["prometheus-cpp"].with_pull = False
        self.options["prometheus-cpp"].with_push = False

        # gRPC: build the C++ code-generator plugin (grpc_cpp_plugin).
        self.options["grpc"].cpp_plugin = True

    # ------------------------------------------------------------------
    # Build layout — standard Conan 2 CMake layout:
    #   build/Debug/   and   build/Release/
    # ------------------------------------------------------------------
    def layout(self):
        cmake_layout(self)

    # ------------------------------------------------------------------
    # Generator — produces conan_toolchain.cmake + CMakeDeps find files
    # ------------------------------------------------------------------
    def generate(self):
        tc = CMakeToolchain(self)

        # Use Ninja everywhere (matches CI and Dockerfile).
        tc.generator = "Ninja"

        for _req, dep in self.dependencies.items():
            # Expose the Conan-installed protoc path explicitly so that
            # proto/CMakeLists.txt does NOT fall back to the system protoc
            # (which may be a different version than the Conan protobuf
            # headers, causing version-check errors in generated .pb.h).
            if dep.ref.name == "protobuf":
                bindirs = dep.cpp_info.bindirs
                if bindirs:
                    protoc = os.path.join(bindirs[0], "protoc")
                    tc.variables["PROTOC_EXECUTABLE"] = (
                        protoc.replace("\\", "/")
                    )

            # Expose the pre-built grpc_cpp_plugin path so
            # proto/CMakeLists.txt can use it in add_custom_command
            # without needing find_program.
            if dep.ref.name == "grpc":
                bindirs = dep.cpp_info.bindirs
                if bindirs:
                    plugin = os.path.join(bindirs[0], "grpc_cpp_plugin")
                    tc.variables["GRPC_CPP_PLUGIN_EXECUTABLE"] = (
                        plugin.replace("\\", "/")
                    )

        # googleapis proto source files are vendored in the source tree;
        # pass the vendored directory to proto/CMakeLists.txt via a
        # CMake cache variable so protoc can resolve the import path.
        tc.variables["GOOGLEAPIS_PROTO_PATH"] = (
            os.path.join(self.source_folder, "proto", "third_party", "googleapis")
            .replace("\\", "/")
        )

        tc.generate()

        deps = CMakeDeps(self)
        deps.generate()
