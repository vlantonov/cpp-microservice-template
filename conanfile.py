from conan import ConanFile
from conan.tools.cmake import CMake, CMakeDeps, CMakeToolchain, cmake_layout


class CppMicroserviceTemplateConan(ConanFile):
    name = "cpp-microservice-template"
    version = "0.1.0"
    package_type = "application"

    settings = "os", "compiler", "build_type", "arch"
    generators = "CMakeToolchain", "CMakeDeps"

    requires = (
        "grpc/1.75.1",
        "protobuf/6.31.1",
        "spdlog/1.15.3",
        "nlohmann_json/3.12.0",
        "prometheus-cpp/1.3.0",
        "opentelemetry-cpp/1.22.0",
        "cpp-httplib/0.27.0",
    )

    default_options = {
        "grpc/*:shared": False,
        "protobuf/*:shared": False,
        "spdlog/*:header_only": False,
        "prometheus-cpp/*:with_push": False,
        "opentelemetry-cpp/*:with_otlp_http": True,
        "opentelemetry-cpp/*:with_otlp_grpc": False,
    }

    def layout(self):
        cmake_layout(self)

    def generate(self):
        tc = CMakeToolchain(self)
        tc.variables["CMAKE_EXPORT_COMPILE_COMMANDS"] = True
        tc.generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()
