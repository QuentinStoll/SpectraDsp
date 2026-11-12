# /MP is only useful with Visual Studio; Ninja already schedules individual files.
# Keep the default conservative: project-level parallelism multiplies this limit.
set(PROFILER_MSVC_COMPILE_JOBS 2 CACHE STRING "MSVC compile processes per project (1 disables /MP)")
if(NOT PROFILER_MSVC_COMPILE_JOBS MATCHES "^[1-9][0-9]*$" OR PROFILER_MSVC_COMPILE_JOBS GREATER 64)
    message(FATAL_ERROR "PROFILER_MSVC_COMPILE_JOBS must be an integer from 1 to 64")
endif()
function(apply_compiler_options target)
    target_compile_features(${target} PRIVATE cxx_std_20)

    if(MSVC)
        if(CMAKE_GENERATOR MATCHES "Visual Studio" AND PROFILER_MSVC_COMPILE_JOBS GREATER 1)
            target_compile_options(${target} PRIVATE /MP${PROFILER_MSVC_COMPILE_JOBS})
        endif()
        target_compile_options(${target} PRIVATE
            /W4
            /utf-8
            $<$<CONFIG:Release>:/O2>
            $<$<CONFIG:Release>:/GL>
        )
        # Static libraries must pass LTO link options to their plugin wrappers.
        target_link_options(${target} PUBLIC
            $<$<CONFIG:Release>:/LTCG>
        )
    else()
        target_compile_options(${target} PRIVATE
            -Wall
            -Wextra
            $<$<CONFIG:Release>:-O3>
            $<$<CONFIG:Debug>:-g>
        )
    endif()

    if(APPLE)
        target_compile_options(${target} PRIVATE
            -Wno-unknown-pragmas
        )
    endif()
endfunction()

function(apply_plugin_definitions target)
    if(ENABLE_ASSERTIONS)
        target_compile_definitions(${target} PRIVATE ENABLE_ASSERTIONS=1)
    endif()

    if(ENABLE_LOGGING)
        target_compile_definitions(${target} PRIVATE ENABLE_LOGGING=1)
    endif()

    if(ENABLE_DSP_TRACE)
        target_compile_definitions(${target} PRIVATE ENABLE_DSP_TRACE=1)
    endif()
endfunction()