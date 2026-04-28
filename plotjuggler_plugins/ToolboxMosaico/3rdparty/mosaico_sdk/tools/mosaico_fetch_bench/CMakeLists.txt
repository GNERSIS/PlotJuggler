add_executable(mosaico_fetch_bench main.cpp)

target_link_libraries(mosaico_fetch_bench PRIVATE
    mosaico_bench_core
)

target_include_directories(mosaico_fetch_bench PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src
)

install(TARGETS mosaico_fetch_bench RUNTIME DESTINATION bin)
