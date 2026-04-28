find_package(GTest REQUIRED)

add_executable(mosaico_unit_tests
    core/time_format_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/core/time_format.cpp
)
target_include_directories(mosaico_unit_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../src
)
target_link_libraries(mosaico_unit_tests PRIVATE GTest::gtest_main)
add_test(NAME mosaico_unit_tests COMMAND mosaico_unit_tests)

include(GoogleTest)

add_executable(ToolboxMosaico_tests test_tls_utils.cpp)
target_link_libraries(ToolboxMosaico_tests PRIVATE GTest::gtest_main Qt5::Core Qt5::Sql arrow::arrow)
target_include_directories(ToolboxMosaico_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/..)
gtest_discover_tests(ToolboxMosaico_tests)

target_sources(ToolboxMosaico_tests PRIVATE
    cache/local_cache_test.cpp
    ui/server_history_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/cache/local_cache.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/ui/server_history.cpp
)
target_include_directories(ToolboxMosaico_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../src
)

# Bench unit tests — pure-logic, no network.
add_executable(mosaico_bench_tests
    bench/proc_self_test.cpp
    bench/ss_poller_test.cpp
    bench/bench_core_test.cpp
    bench/csv_sinks_test.cpp
)
# mosaico_bench_core links arrow::arrow PUBLIC (static + bundled deps).
# The test binary is an executable and must resolve all symbols at link time.
# Arrow::arrow_shared satisfies the Arrow headers without pulling in the
# bundled-dependencies static archive (which has unresolved absl::debian3 symbols).
target_link_libraries(mosaico_bench_tests PRIVATE
    GTest::gtest_main
    mosaico_bench_core
    arrow::arrow
)
gtest_discover_tests(mosaico_bench_tests)
