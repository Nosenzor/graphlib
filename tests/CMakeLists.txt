include(FetchContent)
FetchContent_Declare(
    Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG v3.7.1
)
FetchContent_MakeAvailable(Catch2)

add_executable(graphlib_tests
    unit/test_color.cpp
    unit/test_path.cpp
    unit/test_transforms.cpp
)
target_link_libraries(graphlib_tests PRIVATE graphlib::graphlib Catch2::Catch2WithMain)
# fixtures/fixtures.inc is included relative to the tests dir
target_include_directories(graphlib_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
if(MSVC)
    target_compile_options(graphlib_tests PRIVATE /utf-8)
endif()

list(APPEND CMAKE_MODULE_PATH ${Catch2_SOURCE_DIR}/extras)
include(Catch)
catch_discover_tests(graphlib_tests)
