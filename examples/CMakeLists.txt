# Each example is a tiny executable ending in savefig() — the gallery sources.
set(GRAPHLIB_EXAMPLES
    01_hello_lines
    02_color_cycle
    03_fmt_and_markers
    04_limits_grid
    05_scatter
    06_legend
    07_bar_hist
    08_subplots_dashboard
    09_log_scales
    10_styles_gallery
    11_heatmap_colorbar
    12_contour_gaussian
    13_pcolormesh
    14_scatter_cmap
)

foreach(example IN LISTS GRAPHLIB_EXAMPLES)
    add_executable(${example} ${example}.cpp)
    target_link_libraries(${example} PRIVATE graphlib::graphlib)
endforeach()
