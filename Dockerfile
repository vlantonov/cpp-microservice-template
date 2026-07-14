# syntax=docker/dockerfile:1.7

FROM conanio/gcc14:latest AS build
WORKDIR /src

COPY . .
RUN conan profile detect --force && \
    conan install . --output-folder=build --build=missing -s build_type=Release && \
    cmake -S . -B build/release -G Ninja -DCMAKE_TOOLCHAIN_FILE=build/build/Release/generators/conan_toolchain.cmake -DCMAKE_BUILD_TYPE=Release && \
    cmake --build build/release

FROM gcr.io/distroless/cc-debian12:nonroot
WORKDIR /app
COPY --from=build /src/build/release/microservice /app/microservice
EXPOSE 8080 50051 9464
ENTRYPOINT ["/app/microservice"]
