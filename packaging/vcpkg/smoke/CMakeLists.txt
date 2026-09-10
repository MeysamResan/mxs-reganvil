# Copyright (c) 2026 RegAnvil contributors. SPDX-License-Identifier: MIT
cmake_minimum_required(VERSION 3.20)
project(reganvil_vcpkg_smoke LANGUAGES C)

find_package(mxs_reganvil 0.1.0 CONFIG REQUIRED)
add_executable(reganvil_vcpkg_smoke ../../../tests/consumer/main.c)
target_link_libraries(reganvil_vcpkg_smoke PRIVATE mxs::reganvil)
set_target_properties(reganvil_vcpkg_smoke PROPERTIES
    C_STANDARD 99 C_STANDARD_REQUIRED YES C_EXTENSIONS NO)
enable_testing()
add_test(NAME reganvil.vcpkg_consumer COMMAND reganvil_vcpkg_smoke)
