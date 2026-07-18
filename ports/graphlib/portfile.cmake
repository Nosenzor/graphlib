# graphlib overlay port. Until the port lands in the central vcpkg registry:
#   vcpkg install graphlib --overlay-ports=path/to/graphlib/ports
# REF/SHA512 must point at a published tag; regenerate the hash with
#   curl -L https://github.com/Nosenzor/graphlib/archive/refs/tags/v<X>.tar.gz | shasum -a 512
vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO Nosenzor/graphlib
    REF v1.0.0
    SHA512 3e7b7357d935d1952d25f2c961dd14b67465db1564d0b7e98a1ddf8c1218ae81c2c646157965e39d85961ddda4ded3a7edad7aa82049715925c199fcfe5d73b9
    HEAD_REF main
)

vcpkg_cmake_configure(
    SOURCE_PATH "${SOURCE_PATH}"
    OPTIONS
        -DGRAPHLIB_BUILD_TESTS=OFF
        -DGRAPHLIB_BUILD_EXAMPLES=OFF
        -DGRAPHLIB_INSTALL=ON
)

vcpkg_cmake_install()
vcpkg_cmake_config_fixup(CONFIG_PATH lib/cmake/graphlib)
vcpkg_copy_pdbs()

file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/include"
                    "${CURRENT_PACKAGES_DIR}/debug/share")
vcpkg_install_copyright(FILE_LIST "${SOURCE_PATH}/LICENSE")
