include(FetchContent)

set(FETCHCONTENT_QUIET OFF)
set(FETCHCONTENT_UPDATES_DISCONNECTED ON CACHE BOOL "Skip dependency update checks after the first download" FORCE)

# coverage tools ------------

if(PROFILER_ENABLE_COVERAGE AND WIN32)
    set(PROFILER_OPENCPPCOVERAGE_DIR "${CMAKE_BINARY_DIR}/_deps/opencppcoverage")

    find_program(PROFILER_OPENCPPCOVERAGE_EXE
        NAMES OpenCppCoverage.exe OpenCppCoverage
        PATHS
            "${PROFILER_OPENCPPCOVERAGE_DIR}"
            "$ENV{ProgramFiles}/OpenCppCoverage"
            "C:/Program Files/OpenCppCoverage"
            "C:/Program Files (x86)/OpenCppCoverage"
    )

    if(NOT PROFILER_OPENCPPCOVERAGE_EXE)
        message(FATAL_ERROR
            "Coverage requires an installed OpenCppCoverage. Install it explicitly or set "
            "PROFILER_OPENCPPCOVERAGE_EXE to a trusted executable. Configuration never runs downloaded installers.")
    endif()

    message(STATUS "Coverage: using OpenCppCoverage at ${PROFILER_OPENCPPCOVERAGE_EXE}")
elseif(PROFILER_ENABLE_COVERAGE)
    find_program(PROFILER_LCOV_EXE NAMES lcov)
    find_program(PROFILER_GENHTML_EXE NAMES genhtml)

    if(NOT PROFILER_LCOV_EXE OR NOT PROFILER_GENHTML_EXE)
        message(STATUS "Coverage: fetching lcov")
        FetchContent_Declare(
            lcov
            GIT_REPOSITORY https://github.com/linux-test-project/lcov.git
            GIT_TAG v2.5
            GIT_SHALLOW TRUE
            GIT_PROGRESS TRUE
        )
        FetchContent_Populate(lcov)

        find_program(PROFILER_LCOV_EXE NAMES lcov PATHS "${lcov_SOURCE_DIR}/bin" NO_DEFAULT_PATH)
        find_program(PROFILER_GENHTML_EXE NAMES genhtml PATHS "${lcov_SOURCE_DIR}/bin" NO_DEFAULT_PATH)
    endif()

    if(NOT PROFILER_LCOV_EXE OR NOT PROFILER_GENHTML_EXE)
        message(FATAL_ERROR "lcov/genhtml was not found")
    endif()

    message(STATUS "Coverage: using lcov at ${PROFILER_LCOV_EXE}")
    message(STATUS "Coverage: using genhtml at ${PROFILER_GENHTML_EXE}")
endif()



# JUCE ----------------------

# no option for use of local JUCE (to many breaking changes)
if(NOT TARGET juce::juce_core)
    message(STATUS "Fetching JUCE from repository")
    FetchContent_Declare(
        juce
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 29396c22c93392d6738e021b83196283d6e4d850 # JUCE 8.0.12
        GIT_PROGRESS TRUE
    )
    set(JUCE_BUILD_EXTRAS OFF CACHE BOOL "" FORCE)
    set(JUCE_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(juce)
endif()


# simdjson ------------------

option(USE_SYSTEM_SIMDJSON "Use system-installed simdjson via find_package" OFF) # local builds only
if (TARGET simdjson::simdjson)
    message(STATUS "simdjson: using existing target")
elseif (USE_SYSTEM_SIMDJSON)
    message(STATUS "simdjson: using system package")
    find_package(simdjson CONFIG REQUIRED)
else()
    FetchContent_Declare(
        simdjson
        GIT_REPOSITORY https://github.com/simdjson/simdjson.git
        GIT_TAG 980f2ad3afb12729157b44ed33d2bac41b67b54b # simdjson 4.2.4
    )

    unset(SIMDJSON_BUILD_STATIC CACHE)
    set(BUILD_SHARED_LIBS OFF)
    set(SIMDJSON_DEVELOPER_MODE OFF CACHE BOOL "" FORCE)
    set(SIMDJSON_ENABLE_THREADS ON CACHE BOOL "" FORCE)
    set(CMAKE_POSITION_INDEPENDENT_CODE ON)

    FetchContent_MakeAvailable(simdjson)
endif()


# RTNeural ------------------

if(NOT TARGET RTNeural)
    message(STATUS "Fetching RTNeural")
    FetchContent_Declare(
        rtneural
        GIT_REPOSITORY https://github.com/jatinchowdhury18/RTNeural.git
        GIT_TAG 95c3c0f987a6fe903e7eec71e797405dbed7caf7
        GIT_SUBMODULES "" # xsimd is unused by the Eigen backend
    )

    # Preserve the Eigen backend previously selected implicitly by RTNeural.
    unset(RTNEURAL_BACKEND CACHE)
    unset(RTNEURAL_USE_JSON CACHE)
    set(RTNEURAL_EIGEN ON CACHE BOOL "" FORCE)
    set(RTNEURAL_STL OFF CACHE BOOL "" FORCE)
    set(RTNEURAL_XSIMD OFF CACHE BOOL "" FORCE)

    FetchContent_MakeAvailable(rtneural)
endif()


# tracy ---------------------

option(USE_SYSTEM_TRACY "Use system-installed Tracy via find_package" OFF) # local builds only
if (TARGET Tracy::TracyClient)
    message(STATUS "Tracy: using existing target")
elseif (USE_SYSTEM_TRACY)
    message(STATUS "Tracy: using system package")
    find_package(Tracy CONFIG REQUIRED)
else()
    include(FetchContent)
    FetchContent_Declare(
        tracy
        GIT_REPOSITORY https://github.com/wolfpld/tracy.git
        GIT_TAG 0350df11f89e08687362be68da032889d822ef63 # v0.13.4 do change at your own risk if you have a protocol missmatch with the tracy binaries
    )

    set(TRACY_ENABLE ${ENABLE_TRACY_CLIENT} CACHE BOOL "" FORCE)
    set(TRACY_ON_DEMAND ON CACHE BOOL "" FORCE)
    set(TRACY_ONLY_LOCALHOST ON CACHE BOOL "" FORCE)
    set(TRACY_NO_EXIT OFF CACHE BOOL "" FORCE)
    set(TRACY_NO_CONTEXT_SWITCH OFF CACHE BOOL "" FORCE)
    set(TRACY_NO_SAMPLING OFF CACHE BOOL "" FORCE)
    set(TRACY_NO_SYSTEM_TRACING OFF CACHE BOOL "" FORCE)

    FetchContent_MakeAvailable(tracy)
endif()


# HTTP requests still require libcurl; the embedded browser is disabled.

if(UNIX AND NOT APPLE)
    find_package(CURL REQUIRED)
endif()

# Clear-model amp engine. Neural Amp Modeler Core v0.6.0 (MIT) is fetched as source
# and compiled into spectra_nam. Eigen is the commit pinned by that tag's submodule.
# The archive does not vendor submodules, so Eigen is fetched beside it.
if(POLICY CMP0169)
    cmake_policy(SET CMP0169 OLD)
endif()

set(SPECTRA_NAM_VERSION "0.6.0")
set(SPECTRA_NAM_EIGEN_COMMIT "bc3b39870ecb690a623a3f49149a358b95c5781d")

FetchContent_Declare(
    spectra_nam_src
    URL https://github.com/sdatkinson/NeuralAmpModelerCore/archive/refs/tags/v${SPECTRA_NAM_VERSION}.tar.gz
    URL_HASH SHA256=d55f3c49278a06e5bf5978ea60ae8384603b843d7fdee22a0e9ba71c95108450
    TLS_VERIFY ON
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
FetchContent_Declare(
    spectra_nam_eigen
    URL https://gitlab.com/libeigen/eigen/-/archive/${SPECTRA_NAM_EIGEN_COMMIT}/eigen-${SPECTRA_NAM_EIGEN_COMMIT}.tar.gz
    URL_HASH SHA256=5f6557d1aca6e959a4b955c55f2935261fd30de59c0c79e35bc6d5cbe1de22d1
    TLS_VERIFY ON
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)

FetchContent_GetProperties(spectra_nam_src)
if(NOT spectra_nam_src_POPULATED)
    FetchContent_Populate(spectra_nam_src)
endif()
FetchContent_GetProperties(spectra_nam_eigen)
if(NOT spectra_nam_eigen_POPULATED)
    FetchContent_Populate(spectra_nam_eigen)
endif()

file(GLOB SPECTRA_NAM_SOURCES CONFIGURE_DEPENDS
    "${spectra_nam_src_SOURCE_DIR}/NAM/*.cpp"
    "${spectra_nam_src_SOURCE_DIR}/NAM/*/*.cpp"
)

add_library(spectra_nam STATIC ${SPECTRA_NAM_SOURCES}
    "${CMAKE_CURRENT_SOURCE_DIR}/Source/Src/SpectraNeuralModel.cpp"
)
target_compile_features(spectra_nam PUBLIC cxx_std_20)
set_target_properties(spectra_nam PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_include_directories(spectra_nam PUBLIC
    "${spectra_nam_src_SOURCE_DIR}"
    "${CMAKE_CURRENT_SOURCE_DIR}/Source/Include"
)
target_include_directories(spectra_nam SYSTEM PUBLIC
    "${spectra_nam_eigen_SOURCE_DIR}"
    "${spectra_nam_src_SOURCE_DIR}/Dependencies/nlohmann"
)
target_compile_definitions(spectra_nam PUBLIC NAM_SAMPLE_FLOAT=1 NAM_ENABLE_A2_FAST=1)
if(WIN32)
    target_compile_definitions(spectra_nam PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
endif()
if(MSVC)
    target_compile_options(spectra_nam PRIVATE /W1 /bigobj)
else()
    target_compile_options(spectra_nam PRIVATE -w)
endif()

# Static registration of model architectures lives in this library. Without a
# whole-archive link, the executable drops those objects and every model fails to load.
function(profiler_link_nam_engine target)
    if(MSVC)
        target_link_libraries(${target} PRIVATE spectra_nam)
        target_link_options(${target} PRIVATE "/WHOLEARCHIVE:$<TARGET_FILE:spectra_nam>")
    elseif(APPLE)
        # A comma inside one link item is parsed as $<LINK_ONLY>, so the flag and
        # the library path are two linker arguments.
        target_link_libraries(${target} PRIVATE spectra_nam)
        target_link_options(${target} PRIVATE
            "LINKER:-force_load"
            "LINKER:$<TARGET_FILE:spectra_nam>")
    else()
        target_link_libraries(${target} PRIVATE "-Wl,--whole-archive" spectra_nam "-Wl,--no-whole-archive")
    endif()
endfunction()


