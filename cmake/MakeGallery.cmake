# cmake -P script: assemble docs site gallery.html from docs/gallery/*.svg,
# linking each figure to the example source that produced it. Zero non-CMake
# dependencies so the docs target works on a clean checkout.
#
# Expects: -DGALLERY_DIR=... -DEXAMPLES_DIR=... -DOUT_HTML=...

file(GLOB svgs "${GALLERY_DIR}/*.svg")
list(SORT svgs)
file(GLOB examples RELATIVE "${EXAMPLES_DIR}" "${EXAMPLES_DIR}/*.cpp")

set(cards "")
foreach(svg IN LISTS svgs)
    get_filename_component(stem "${svg}" NAME_WE)
    get_filename_component(fname "${svg}" NAME)
    set(source_link "")
    foreach(ex IN LISTS examples)
        string(FIND "${ex}" "${stem}" hit)
        if(NOT hit EQUAL -1)
            set(source_link "<a class=\"src\" href=\"https://github.com/Nosenzor/graphlib/blob/main/examples/${ex}\">${ex}</a>")
            break()
        endif()
    endforeach()
    string(REPLACE "_" " " title "${stem}")
    string(APPEND cards "<figure><a href=\"gallery/${fname}\"><img loading=\"lazy\" src=\"gallery/${fname}\" alt=\"${title}\"/></a><figcaption>${title} ${source_link}</figcaption></figure>\n")
endforeach()

set(html "<!DOCTYPE html>
<html lang=\"en\"><head><meta charset=\"utf-8\"/>
<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\"/>
<title>graphlib gallery</title>
<link rel=\"stylesheet\" href=\"site.css\"/></head>
<body>
<nav><a href=\"index.html\">graphlib</a> · <a href=\"quickstart.html\">quick start</a> · <strong>gallery</strong> · <a href=\"api/index.html\">API</a></nav>
<h1>Gallery</h1>
<p>Every figure below is produced by one short C++ file in <code>examples/</code>, rendered by graphlib's own SVG backend.</p>
<main class=\"grid\">
${cards}</main>
</body></html>
")
file(WRITE "${OUT_HTML}" "${html}")
message(STATUS "gallery.html: ${OUT_HTML}")
