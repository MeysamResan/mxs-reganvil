cmake_minimum_required(VERSION 3.20)
project(mxs_reganvil_consumer LANGUAGES C)

if(MXS_REGANVIL_SOURCE_DIR)
    add_subdirectory("${MXS_REGANVIL_SOURCE_DIR}" reganvil)
    if(MXS_REGANVIL_BUILD_TESTS OR MXS_REGANVIL_BUILD_EXAMPLES)
        message(FATAL_ERROR "Embedding RegAnvil must default to the library alone")
    endif()
    foreach(unwanted_target IN ITEMS mxs_reganvil_tests mxs_reganvil_driver_tests
            mxs_reganvil_public_consumer mxs_reganvil_demo)
        if(TARGET ${unwanted_target})
            message(FATAL_ERROR "Embedding RegAnvil unexpectedly added ${unwanted_target}")
        endif()
    endforeach()
else()
    find_package(mxs_reganvil 0.1.0 CONFIG REQUIRED)
endif()

# Consumers choose their own warning and instrumentation policy. Check both the
# source target and the installed export instead of compensating for leaked flags.
foreach(interface_property IN ITEMS INTERFACE_COMPILE_OPTIONS INTERFACE_LINK_OPTIONS)
    get_target_property(interface_options mxs::reganvil ${interface_property})
    if(interface_options)
        message(FATAL_ERROR "RegAnvil leaked ${interface_property}: ${interface_options}")
    endif()
endforeach()

add_executable(mxs_reganvil_consumer main.c)
target_link_libraries(mxs_reganvil_consumer PRIVATE mxs::reganvil)
set_target_properties(mxs_reganvil_consumer PROPERTIES
    C_STANDARD 99 C_STANDARD_REQUIRED YES C_EXTENSIONS NO)
if(MSVC)
    target_compile_options(mxs_reganvil_consumer PRIVATE /std:c11)
endif()
if(MXS_REGANVIL_CONSUMER_SANITIZERS)
    # An instrumented static archive needs its final executable to link the
    # runtimes explicitly. The installed library exports no sanitizer flags.
    target_compile_options(mxs_reganvil_consumer PRIVATE
        -fsanitize=address,undefined -fno-omit-frame-pointer)
    target_link_options(mxs_reganvil_consumer PRIVATE -fsanitize=address,undefined)
endif()
enable_testing()
add_test(NAME consumer.runs COMMAND mxs_reganvil_consumer)
