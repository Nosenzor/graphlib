include(FetchContent)
FetchContent_Declare(
    Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG v3.7.1
)
FetchContent_MakeAvailable(Catch2)

add_executable(graphlib_tests
    image/svg_golden.cpp
    unit/test_axes.cpp
    unit/test_color.cpp
    unit/test_path.cpp
    unit/test_text_metrics.cpp
    unit/test_ticker.cpp
    unit/test_transforms.cpp
)
target_link_libraries(graphlib_tests PRIVATE graphlib::graphlib Catch2::Catch2WithMain)
# fixtures/fixtures.inc is included relative to the tests dir; internal
# headers (text/font_manager.hpp) are white-box-tested from src/.
target_include_directories(graphlib_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../src)
target_compile_definitions(graphlib_tests PRIVATE
    GRAPHLIB_TESTS_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
if(MSVC)
    target_compile_options(graphlib_tests PRIVATE /utf-8)
endif()

# Regenerate golden baselines (review with `git diff` before committing!)
add_custom_target(update-baselines
    COMMAND ${CMAKE_COMMAND} -E env GRAPHLIB_UPDATE_BASELINES=1
            $<TARGET_FILE:graphlib_tests> "[golden]"
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMENT "Regenerating golden SVG baselines")

list(APPEND CMAKE_MODULE_PATH ${Catch2_SOURCE_DIR}/extras)
include(Catch)
catch_discover_tests(graphlib_tests)
