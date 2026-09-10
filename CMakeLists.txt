cmake_minimum_required(VERSION 3.20)
project(mxs_reganvil VERSION 0.1.0 DESCRIPTION "RegAnvil: Hardware register behavior. Plain C tests." LANGUAGES C)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

set(MXS_REGANVIL_TOP_LEVEL OFF)
if(CMAKE_SOURCE_DIR STREQUAL PROJECT_SOURCE_DIR)
    set(MXS_REGANVIL_TOP_LEVEL ON)
endif()
option(MXS_REGANVIL_BUILD_TESTS "Build RegAnvil behavior and integration tests" ${MXS_REGANVIL_TOP_LEVEL})
option(MXS_REGANVIL_BUILD_EXAMPLES "Build the RegAnvil interrupt acknowledgement demonstration" ${MXS_REGANVIL_TOP_LEVEL})
option(MXS_REGANVIL_ENABLE_SANITIZERS "Enable private AddressSanitizer and UndefinedBehaviorSanitizer instrumentation" OFF)

if(MXS_REGANVIL_ENABLE_SANITIZERS)
    if(NOT CMAKE_C_COMPILER_ID MATCHES "^(GNU|Clang|AppleClang)$" OR MSVC)
        message(FATAL_ERROR "RegAnvil ASan/UBSan requires a supported GCC or Clang driver")
    endif()
    include(CheckCSourceCompiles)
    include(CMakePushCheckState)
    cmake_push_check_state(RESET)
    set(CMAKE_REQUIRED_FLAGS "-fsanitize=address,undefined -fno-omit-frame-pointer")
    set(CMAKE_REQUIRED_LINK_OPTIONS "-fsanitize=address,undefined")
    check_c_source_compiles("int main(void) { return 0; }" MXS_REGANVIL_HAVE_SANITIZERS)
    cmake_pop_check_state()
    if(NOT MXS_REGANVIL_HAVE_SANITIZERS)
        message(FATAL_ERROR "The selected compiler cannot compile and link ASan/UBSan; install its runtimes or disable MXS_REGANVIL_ENABLE_SANITIZERS")
    endif()
endif()

function(mxs_reganvil_configure_target target)
    set_target_properties(${target} PROPERTIES C_STANDARD 99 C_STANDARD_REQUIRED YES C_EXTENSIONS NO)
    if(MSVC)
        # MSVC has no C99 switch; its C11 mode supports the C99 subset used here.
        target_compile_options(${target} PRIVATE /W4 /std:c11)
    elseif(CMAKE_C_COMPILER_ID MATCHES "^(GNU|Clang|AppleClang)$")
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wshadow)
    endif()
    if(MXS_REGANVIL_ENABLE_SANITIZERS)
        target_compile_options(${target} PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
        target_link_options(${target} PRIVATE -fsanitize=address,undefined)
    endif()
endfunction()

add_library(mxs_reganvil STATIC src/mxs_reganvil.c)
add_library(mxs::reganvil ALIAS mxs_reganvil)
set_target_properties(mxs_reganvil PROPERTIES EXPORT_NAME reganvil)
target_include_directories(mxs_reganvil PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
mxs_reganvil_configure_target(mxs_reganvil)

if(MXS_REGANVIL_BUILD_EXAMPLES)
    add_executable(mxs_reganvil_demo examples/interrupt_ack.c examples/interrupt_driver.c)
    target_link_libraries(mxs_reganvil_demo PRIVATE mxs::reganvil)
    mxs_reganvil_configure_target(mxs_reganvil_demo)
endif()

install(TARGETS mxs_reganvil EXPORT mxs_reganvilTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
install(FILES include/mxs_reganvil.h DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
set(MXS_REGANVIL_INSTALL_CMAKEDIR "${CMAKE_INSTALL_LIBDIR}/cmake/mxs_reganvil")
install(EXPORT mxs_reganvilTargets FILE mxs_reganvilTargets.cmake
    NAMESPACE mxs:: DESTINATION ${MXS_REGANVIL_INSTALL_CMAKEDIR})
configure_package_config_file(cmake/mxs_reganvilConfig.cmake.in
    "${CMAKE_CURRENT_BINARY_DIR}/mxs_reganvilConfig.cmake"
    INSTALL_DESTINATION ${MXS_REGANVIL_INSTALL_CMAKEDIR})
write_basic_package_version_file("${CMAKE_CURRENT_BINARY_DIR}/mxs_reganvilConfigVersion.cmake"
    VERSION ${PROJECT_VERSION} COMPATIBILITY SameMinorVersion)
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/mxs_reganvilConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/mxs_reganvilConfigVersion.cmake"
    DESTINATION ${MXS_REGANVIL_INSTALL_CMAKEDIR})
install(FILES LICENSE DESTINATION "${CMAKE_INSTALL_DATADIR}/mxs_reganvil")

if(MXS_REGANVIL_BUILD_TESTS)
    include(CTest)
    if(BUILD_TESTING)
        add_executable(mxs_reganvil_tests tests/test_reganvil.c)
        target_link_libraries(mxs_reganvil_tests PRIVATE mxs::reganvil)
        mxs_reganvil_configure_target(mxs_reganvil_tests)
        add_test(NAME reganvil.behavior COMMAND mxs_reganvil_tests)
        add_executable(mxs_reganvil_driver_tests
            tests/driver/rp2040_irq_driver.c tests/driver/test_rp2040_irq_driver.c)
        target_link_libraries(mxs_reganvil_driver_tests PRIVATE mxs::reganvil)
        mxs_reganvil_configure_target(mxs_reganvil_driver_tests)
        add_test(NAME reganvil.driver_integration COMMAND mxs_reganvil_driver_tests)

        add_executable(mxs_reganvil_public_consumer tests/consumer/main.c)
        target_link_libraries(mxs_reganvil_public_consumer PRIVATE mxs::reganvil)
        mxs_reganvil_configure_target(mxs_reganvil_public_consumer)
        add_test(NAME reganvil.public_consumer COMMAND mxs_reganvil_public_consumer)
        if(MXS_REGANVIL_BUILD_EXAMPLES)
            add_test(NAME reganvil.demo COMMAND mxs_reganvil_demo)
        endif()

        configure_file(cmake/VerifyConsumer.cmake.in
            "${CMAKE_CURRENT_BINARY_DIR}/VerifyConsumer.cmake" @ONLY)
        foreach(mode IN ITEMS source installed)
            add_test(NAME reganvil.${mode}_consumer COMMAND "${CMAKE_COMMAND}"
                "-DMXS_REGANVIL_CONSUMER_MODE=${mode}"
                "-DMXS_REGANVIL_TEST_CONFIG=$<CONFIG>"
                -P "${CMAKE_CURRENT_BINARY_DIR}/VerifyConsumer.cmake")
            set_tests_properties(reganvil.${mode}_consumer PROPERTIES TIMEOUT 180)
        endforeach()
    endif()
endif()
