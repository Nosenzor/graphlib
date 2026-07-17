# find_package consumer smoke (v1.0 packaging exit criterion): built by CI
# against an installed prefix — and usable manually:
#   cmake --install build/release --prefix /tmp/gl
#   cmake -B build -S tests/package -DCMAKE_PREFIX_PATH=/tmp/gl && cmake --build build
cmake_minimum_required(VERSION 3.24)
project(graphlib_package_smoke CXX)
find_package(graphlib 1.0 REQUIRED)
add_executable(package_smoke main.cpp)
target_link_libraries(package_smoke PRIVATE graphlib::graphlib)
