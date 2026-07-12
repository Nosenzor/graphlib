# Each example is a tiny executable ending in savefig() — the gallery sources.
set(GRAPHLIB_EXAMPLES
    01_hello_lines
    02_color_cycle
    03_fmt_and_markers
    04_limits_grid
)

foreach(example IN LISTS GRAPHLIB_EXAMPLES)
    add_executable(${example} ${example}.cpp)
    target_link_libraries(${example} PRIVATE graphlib::graphlib)
endforeach()
