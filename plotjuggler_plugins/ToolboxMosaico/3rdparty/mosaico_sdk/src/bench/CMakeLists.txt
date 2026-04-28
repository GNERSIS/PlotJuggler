# mosaico_bench_core — instrumentation engine + sinks for fetch benchmarks.
# No Qt. Linked by the CLI binary (mosaico_fetch_bench) and by the
# ToolboxMosaico plugin's PlotDataMapSink.

add_library(mosaico_bench_core STATIC
    bench_core.cpp
    csv_sinks.cpp
    proc_self.cpp
    ss_poller.cpp
)

target_include_directories(mosaico_bench_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)

# mosaico_sdk is intentionally deferred until Task 11 when runInstrumentedFetch
# gains its real implementation and requires the full Arrow/Flight graph.
# For now, bench_core.hpp only forward-declares mosaico::MosaicoClient and
# exposes arrow::Status — both of which only require Arrow::arrow_shared.
#
# Arrow::arrow_shared is used (not arrow::arrow / Arrow::arrow_static) because
# the bench tests are executables that must fully resolve all symbols at link
# time, and libarrow_bundled_dependencies.a has unresolved absl::debian3 symbols
# that are only satisfied in a shared-lib context.
target_link_libraries(mosaico_bench_core PUBLIC
    Arrow::arrow_shared
    Threads::Threads
)
target_link_libraries(mosaico_bench_core PRIVATE
    mosaico_sdk
)
