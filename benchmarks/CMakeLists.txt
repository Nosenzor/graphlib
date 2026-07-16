# Google Benchmark is dev-only (AGENTS.md dependency tier: FetchContent).
include(FetchContent)
set(BENCHMARK_ENABLE_TESTING OFF CACHE BOOL "" FORCE)
set(BENCHMARK_ENABLE_GTEST_TESTS OFF CACHE BOOL "" FORCE)
set(BENCHMARK_ENABLE_INSTALL OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
    googlebenchmark
    GIT_REPOSITORY https://github.com/google/benchmark.git
    GIT_TAG v1.9.1
)
FetchContent_MakeAvailable(googlebenchmark)

add_executable(graphlib_bench bench_render.cpp)
target_link_libraries(graphlib_bench PRIVATE graphlib::graphlib benchmark::benchmark)
# White-box access (internal renderer headers) mirrors tests/.
target_include_directories(graphlib_bench PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src)
