cmake_minimum_required(VERSION 3.22) # same as JUCE

project(Profiler VERSION 0.0.0 LANGUAGES C CXX)

set(SPECTRA_RELEASE_TAG "0.0.0-dev" CACHE STRING "GitHub release tag baked into the app")
if(DEFINED ENV{SPECTRA_RELEASE_TAG} AND NOT "$ENV{SPECTRA_RELEASE_TAG}" STREQUAL "")
    set(SPECTRA_RELEASE_TAG "$ENV{SPECTRA_RELEASE_TAG}" CACHE STRING "GitHub release tag baked into the app" FORCE)
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(PROFILER_BUILD_TESTS "Build Profiler automated tests" ON)
option(PROFILER_ENABLE_COVERAGE "Run Profiler tests with coverage reporting" OFF)
set(PROFILER_COVERAGE_MINIMUM 70 CACHE STRING "Minimum required line coverage percentage")

if(NOT PROFILER_COVERAGE_MINIMUM MATCHES "^[0-9]+$")
    message(FATAL_ERROR "PROFILER_COVERAGE_MINIMUM must be an integer from 0 to 100")
endif()

if(PROFILER_COVERAGE_MINIMUM LESS 0 OR PROFILER_COVERAGE_MINIMUM GREATER 100)
    message(FATAL_ERROR "PROFILER_COVERAGE_MINIMUM must be an integer from 0 to 100")
endif()

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

include(spectra_dsp)
include(presets)
include(dependencies)
include(juce_setup)
include(compiler_options)

add_subdirectory(Source)

if(PROFILER_BUILD_TESTS)
    enable_testing()
    add_subdirectory(Tests)
endif()
