# ---------------------------------------------------------------------------
# tests/CMakeLists.txt
# Defines: hello_tests (EXECUTABLE) — §2.4
# ---------------------------------------------------------------------------

add_executable(hello_tests
    hello_service_test.cpp
    logger_test.cpp
    metrics_test.cpp
    tracer_test.cpp
)

target_link_libraries(hello_tests
    PRIVATE
        hello_lib
        proto_gen
        GTest::gtest_main
        httplib::httplib   # used in metrics/service integration tests
)

add_test(NAME hello_tests COMMAND hello_tests)
