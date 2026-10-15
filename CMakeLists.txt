cmake_minimum_required(VERSION 3.20)
project(markov VERSION 1.0.0 DESCRIPTION "Generic Markov chain engine in C99"
        LANGUAGES C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(MK_WERROR "Treat compiler warnings as errors" ON)
option(MK_SANITIZE "Build with AddressSanitizer + UndefinedBehaviorSanitizer" OFF)
option(MK_COVERAGE "Instrument for gcov coverage" OFF)
option(MK_BUILD_APPS "Build the command-line apps" ON)
option(MK_BUILD_LEGACY "Build the original assignment programs" ON)
option(MK_BUILD_TESTS "Build the test suites" ON)
option(MK_BUILD_BENCH "Build the benchmarks" ON)
option(MK_LIBFUZZER "Also build a libFuzzer binary (clang only)" OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING "Build type" FORCE)
endif()

include(cmake/Warnings.cmake)

if(MK_SANITIZE)
    add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer
                        -fno-sanitize-recover=all)
    add_link_options(-fsanitize=address,undefined)
endif()
if(MK_COVERAGE)
    add_compile_options(--coverage -O0)
    add_link_options(--coverage)
endif()

add_subdirectory(engine)
add_subdirectory(wasm)

if(EMSCRIPTEN)
    # The browser build only needs the engine and its JavaScript bridge.
    return()
endif()

if(MK_BUILD_APPS)
    add_subdirectory(apps/cli)
endif()
if(MK_BUILD_LEGACY)
    add_subdirectory(legacy)
endif()
if(MK_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
if(MK_BUILD_BENCH)
    add_subdirectory(bench)
endif()
