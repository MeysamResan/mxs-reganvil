# Copyright (c) 2026 RegAnvil contributors. SPDX-License-Identifier: MIT
vcpkg_check_linkage(ONLY_STATIC_LIBRARY)

# Use the tested release asset, independently of changes in the working tree.
vcpkg_download_distfile(ARCHIVE
    URLS "https://github.com/MeysamResan/mxs-reganvil/releases/download/v${VERSION}/mxs-reganvil-${VERSION}-source.zip"
    FILENAME "mxs-reganvil-${VERSION}-source.zip"
    SHA512 2760b0e76fb69b3d0a45e4e44097e1d3bd912da3a8392bd4df18e3ad495853f07826c4b8bbfdf11ea83ded473f4bec15330c67f82c1276f833cfd945f057f337
)
vcpkg_extract_source_archive(SOURCE_PATH ARCHIVE "${ARCHIVE}")

vcpkg_cmake_configure(
    SOURCE_PATH "${SOURCE_PATH}"
    OPTIONS
        -DMXS_REGANVIL_BUILD_TESTS=OFF
        -DMXS_REGANVIL_BUILD_EXAMPLES=OFF
        -DMXS_REGANVIL_ENABLE_SANITIZERS=OFF
)
vcpkg_cmake_install()
vcpkg_cmake_config_fixup(
    PACKAGE_NAME mxs_reganvil
    CONFIG_PATH lib/cmake/mxs_reganvil
)

# The shared header/configs serve both build configurations. License metadata
# belongs under the port name; the upstream CMake package keeps its own name.
file(REMOVE_RECURSE
    "${CURRENT_PACKAGES_DIR}/debug/include"
    "${CURRENT_PACKAGES_DIR}/debug/share"
)
file(REMOVE "${CURRENT_PACKAGES_DIR}/share/mxs_reganvil/LICENSE")
vcpkg_install_copyright(FILE_LIST "${SOURCE_PATH}/LICENSE")
file(INSTALL "${CMAKE_CURRENT_LIST_DIR}/usage"
    DESTINATION "${CURRENT_PACKAGES_DIR}/share/${PORT}")
