cmake_minimum_required(VERSION 3.24)

project(graphlib
    VERSION 0.3.0
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

# Vendored AGG 2.4 (matplotlib's patched copy — third_party/README.md).
# Exactly the translation units matplotlib compiles; warnings suppressed
# (tier-1 vendored code is never edited, so its warnings are not actionable).
set(GRAPHLIB_AGG_SOURCES
    third_party/agg/src/agg_bezier_arc.cpp
    third_party/agg/src/agg_curves.cpp
    third_party/agg/src/agg_image_filters.cpp
    third_party/agg/src/agg_trans_affine.cpp
    third_party/agg/src/agg_vcgen_contour.cpp
    third_party/agg/src/agg_vcgen_dash.cpp
    third_party/agg/src/agg_vcgen_stroke.cpp
    third_party/agg/src/agg_vpgen_segmentator.cpp
)
set_source_files_properties(${GRAPHLIB_AGG_SOURCES} PROPERTIES
    COMPILE_OPTIONS "$<IF:$<CXX_COMPILER_ID:MSVC>,/w,-w>")
# GCC's -Wmaybe-uninitialized is a middle-end warning that escapes -isystem
# suppression when AGG header templates inline at -O3 (false positive in
# agg_curves.h). Vendored code is never edited — suppress in the including TU.
set_source_files_properties(src/backends/agg/agg_renderer.cpp PROPERTIES
    COMPILE_OPTIONS "$<$<CXX_COMPILER_ID:GNU>:-Wno-maybe-uninitialized>")

# Embedded DejaVu fonts (build-time generation; no runtime file lookups).
include(cmake/EmbedResource.cmake)
graphlib_embed_resource("${CMAKE_CURRENT_SOURCE_DIR}/third_party/fonts/DejaVuSans.ttf"
    "${CMAKE_CURRENT_BINARY_DIR}/generated/fonts/dejavu_sans.cpp" graphlib_font_dejavu_sans)
graphlib_embed_resource("${CMAKE_CURRENT_SOURCE_DIR}/third_party/fonts/DejaVuSans-Bold.ttf"
    "${CMAKE_CURRENT_BINARY_DIR}/generated/fonts/dejavu_sans_bold.cpp"
    graphlib_font_dejavu_sans_bold)

add_library(graphlib STATIC
    ${GRAPHLIB_AGG_SOURCES}
    "${CMAKE_CURRENT_BINARY_DIR}/generated/fonts/dejavu_sans.cpp"
    "${CMAKE_CURRENT_BINARY_DIR}/generated/fonts/dejavu_sans_bold.cpp"
    src/backends/agg/agg_renderer.cpp
    src/backends/svg/svg_renderer.cpp
    src/core/axes.cpp
    src/core/axis.cpp
    src/core/collections.cpp
    src/core/color.cpp
    src/core/colormaps.cpp
    src/core/figure.cpp
    src/core/legend.cpp
    src/core/graphics_context.cpp
    src/core/gridspec.cpp
    src/core/image.cpp
    src/core/lines.cpp
    src/core/markers.cpp
    src/core/patches.cpp
    src/core/path.cpp
    src/core/pyplot.cpp
    src/core/rc.cpp
    src/core/renderer.cpp
    src/core/text.cpp
    src/core/ticker.cpp
    src/core/transforms.cpp
    src/text/font_manager.cpp
    src/text/stb_impl.cpp
)
set_source_files_properties(src/text/stb_impl.cpp PROPERTIES
    COMPILE_OPTIONS "$<IF:$<CXX_COMPILER_ID:MSVC>,/w,-w>")
add_library(graphlib::graphlib ALIAS graphlib)

target_compile_features(graphlib PUBLIC cxx_std_20)
set_target_properties(graphlib PROPERTIES CXX_EXTENSIONS OFF)

configure_file(cmake/version.hpp.in generated/graphlib/version.hpp @ONLY)
target_include_directories(graphlib PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/generated>
)
# Vendored headers stay an implementation detail (ground rule: backends never
# leak); SYSTEM keeps our warning flags away from them.
target_include_directories(graphlib SYSTEM PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/agg/include
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/stb
)
# Internal headers (src/text/..., src/core/fmt.hpp) — never installed.
target_include_directories(graphlib PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

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
