cmake_minimum_required(VERSION 3.24)

project(graphlib
    VERSION 0.1.0
    DESCRIPTION "A modern C++ plotting library that mimics matplotlib"
    HOMEPAGE_URL "https://github.com/Nosenzor/graphlib"
    LANGUAGES CXX)

option(GRAPHLIB_BUILD_TESTS "Build graphlib tests" ${PROJECT_IS_TOP_LEVEL})
option(GRAPHLIB_BUILD_EXAMPLES "Build graphlib examples" ${PROJECT_IS_TOP_LEVEL})
option(GRAPHLIB_WERROR "Treat warnings as errors (CI)" OFF)

# Project-wide baseline (also applies to FetchContent deps like Catch2, so
# their ABI matches ours — e.g. string_view stringification).
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(graphlib STATIC
    src/backends/svg/svg_renderer.cpp
    src/core/axes.cpp
    src/core/axis.cpp
    src/core/color.cpp
    src/core/figure.cpp
    src/core/graphics_context.cpp
    src/core/lines.cpp
    src/core/markers.cpp
    src/core/path.cpp
    src/core/pyplot.cpp
    src/core/renderer.cpp
    src/core/text.cpp
    src/core/ticker.cpp
    src/core/transforms.cpp
)
add_library(graphlib::graphlib ALIAS graphlib)

target_compile_features(graphlib PUBLIC cxx_std_20)
set_target_properties(graphlib PROPERTIES CXX_EXTENSIONS OFF)

configure_file(cmake/version.hpp.in generated/graphlib/version.hpp @ONLY)
target_include_directories(graphlib PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/generated>
)

if(MSVC)
    target_compile_options(graphlib PRIVATE /W4 /permissive- /utf-8)
    if(GRAPHLIB_WERROR)
        target_compile_options(graphlib PRIVATE /WX)
    endif()
else()
    target_compile_options(graphlib PRIVATE -Wall -Wextra -Wpedantic -Wshadow -Wconversion)
    if(GRAPHLIB_WERROR)
        target_compile_options(graphlib PRIVATE -Werror)
    endif()
endif()

if(GRAPHLIB_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

if(GRAPHLIB_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()
