set(PROFILER_TEST_SOURCES
    TestRunner.cpp
    AmpCaptureTests.cpp
    AudioProcessorTests.cpp
    ProfileProcessorIntegrationTests.cpp
    ProfileManagerTests.cpp
    PresetCatalogTests.cpp
    LoggingTests.cpp
    NotificationBannerTests.cpp
    UiSettingsTests.cpp
    MarketplaceClientTests.cpp
    AppUpdateTests.cpp
    SpectraEngineTests.cpp
    FxQualityTests.cpp
    AmpConditionTests.cpp
    AudioContractTests.cpp
)

add_executable(ProfilerTests EXCLUDE_FROM_ALL
    ${PROFILER_TEST_SOURCES}
    ../Source/Src/Components/NotificationBanner.cpp
    ../Source/Src/Logging.cpp
    ../Source/Src/AmpCapture.cpp
    ../Source/Src/PluginProcessor.cpp
    ../Source/Src/Fx/FxCatalog.cpp
    ../Source/Src/Fx/PedalDrive.cpp
    ../Source/Src/Fx/FxDsp.cpp
    ../Source/Src/Fx/FxRack.cpp
    ../Source/Src/ProfileManager.cpp
    ../Source/Src/PresetManager.cpp
    ../Source/Src/ProfileCatalog.cpp
    ../Source/Src/UiSettings.cpp
    ../Source/Src/MarketplaceClient.cpp
    ../Source/Src/AppUpdate.cpp
    ../Source/Src/SpectraEngine.cpp
    ../Source/Src/SpectraAmp.cpp
    ../Source/Src/AmpCondition.cpp
)

target_include_directories(ProfilerTests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/Support
    ${CMAKE_SOURCE_DIR}/Source/Include
)

target_compile_definitions(ProfilerTests PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_UNIT_TESTS=1
    PROFILER_HEADLESS_TESTS=1
    JucePlugin_IsSynth=0
    JucePlugin_WantsMidiInput=0
    JucePlugin_ProducesMidiOutput=0
    JucePlugin_IsMidiEffect=0
    JucePlugin_Name="Spectra"
    SPECTRA_RELEASE_TAG="v2026.11.06-100"
    PROFILER_TEST_TEMP_ROOT="${CMAKE_CURRENT_BINARY_DIR}/tmp"
)

target_link_libraries(ProfilerTests PRIVATE
    juce::juce_audio_basics
    juce::juce_audio_formats
    juce::juce_audio_processors
    juce::juce_core
    juce::juce_data_structures
    juce::juce_dsp
    juce::juce_events
    juce::juce_graphics
    juce::juce_gui_basics
    ProfilerAssets
    RTNeural
    simdjson
    TracyClient
)

if(UNIX AND NOT APPLE)
    target_link_libraries(ProfilerTests PRIVATE CURL::libcurl)
endif()

# Treat dependency headers as external, retaining warnings for our source files.
target_include_directories(ProfilerTests SYSTEM PRIVATE
    $<TARGET_PROPERTY:simdjson::simdjson,INTERFACE_INCLUDE_DIRECTORIES>
    $<TARGET_PROPERTY:RTNeural,INTERFACE_INCLUDE_DIRECTORIES>
)

apply_compiler_options(ProfilerTests)
profiler_link_nam_engine(ProfilerTests)
profiler_attach_spectra_dsp(ProfilerTests)

if(PROFILER_ENABLE_COVERAGE)
    set(PROFILER_COVERAGE_DIR "${CMAKE_BINARY_DIR}/coverage")

    target_compile_definitions(ProfilerTests PRIVATE
        JUCE_DISABLE_ASSERTIONS=1
        JUCE_LOG_ASSERTIONS=0
    )

    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
        target_compile_options(ProfilerTests PRIVATE -O0 -g --coverage)
        target_link_options(ProfilerTests PRIVATE --coverage)
    endif()

    if(MSVC)
        target_compile_options(ProfilerTests PRIVATE /Od /Zi /FS)
        target_link_options(ProfilerTests PRIVATE /DEBUG:FULL /INCREMENTAL:NO)
    endif()

    if(WIN32)
        file(TO_NATIVE_PATH "${CMAKE_SOURCE_DIR}/Source" PROFILER_COVERAGE_SOURCE_DIR)
        file(TO_NATIVE_PATH "${PROFILER_COVERAGE_DIR}/html" PROFILER_COVERAGE_HTML_DIR)
        file(TO_NATIVE_PATH "${PROFILER_COVERAGE_DIR}/coverage.xml" PROFILER_COVERAGE_XML)

        add_custom_target(ProfilerCoverage
            COMMAND "${CMAKE_COMMAND}" -E make_directory "${PROFILER_COVERAGE_DIR}"
            COMMAND "${PROFILER_OPENCPPCOVERAGE_EXE}"
                --quiet
                --cover_children
                --modules "*ProfilerTests.exe"
                --sources "${PROFILER_COVERAGE_SOURCE_DIR}"
                --export_type "html:${PROFILER_COVERAGE_HTML_DIR}"
                --export_type "cobertura:${PROFILER_COVERAGE_XML}"
                -- "${CMAKE_CTEST_COMMAND}" --test-dir "${CMAKE_BINARY_DIR}" -C "$<CONFIG>" --output-on-failure
            COMMAND "${CMAKE_COMMAND}"
                -DPROFILER_COVERAGE_XML="${PROFILER_COVERAGE_DIR}/coverage.xml"
                -DPROFILER_COVERAGE_MINIMUM=${PROFILER_COVERAGE_MINIMUM}
                -P "${CMAKE_SOURCE_DIR}/cmake/PrintCoverageSummary.cmake"
            COMMAND "${CMAKE_COMMAND}" -E echo "Coverage report: ${PROFILER_COVERAGE_DIR}/html/index.html"
            DEPENDS ProfilerTests
            WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
            USES_TERMINAL
        )
    else()
        add_custom_target(ProfilerCoverage
            COMMAND "${CMAKE_COMMAND}" -E make_directory "${PROFILER_COVERAGE_DIR}"
            COMMAND "${PROFILER_LCOV_EXE}" --directory "${CMAKE_BINARY_DIR}" --zerocounters
            COMMAND "${CMAKE_CTEST_COMMAND}" --test-dir "${CMAKE_BINARY_DIR}" -C "$<CONFIG>" --output-on-failure
            COMMAND "${PROFILER_LCOV_EXE}" --directory "${CMAKE_BINARY_DIR}" --capture --output-file "${PROFILER_COVERAGE_DIR}/coverage.info"
            COMMAND "${PROFILER_LCOV_EXE}" --remove "${PROFILER_COVERAGE_DIR}/coverage.info"
                "*/Tests/*"
                "*/build/*"
                "*/_deps/*"
                "*/JuceLibraryCode/*"
                --output-file "${PROFILER_COVERAGE_DIR}/coverage.filtered.info"
            COMMAND "${PROFILER_LCOV_EXE}" --summary "${PROFILER_COVERAGE_DIR}/coverage.filtered.info"
            COMMAND "${PROFILER_GENHTML_EXE}" "${PROFILER_COVERAGE_DIR}/coverage.filtered.info" --output-directory "${PROFILER_COVERAGE_DIR}/html"
            COMMAND "${CMAKE_COMMAND}"
                -DPROFILER_COVERAGE_LCOV="${PROFILER_COVERAGE_DIR}/coverage.filtered.info"
                -DPROFILER_COVERAGE_MINIMUM=${PROFILER_COVERAGE_MINIMUM}
                -P "${CMAKE_SOURCE_DIR}/cmake/PrintCoverageSummary.cmake"
            COMMAND "${CMAKE_COMMAND}" -E echo "Coverage report: ${PROFILER_COVERAGE_DIR}/html/index.html"
            DEPENDS ProfilerTests
            WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
            USES_TERMINAL
        )
    endif()
endif()

# Run the whole registered JUCE category once: no hand-maintained list can omit
# cases, and AudioContract emits its complete metrics in this same invocation.
add_test(NAME ProfilerTests.All COMMAND ProfilerTests)
set_tests_properties(ProfilerTests.All PROPERTIES
    WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
    ENVIRONMENT "PROFILER_AUDIO_METRICS=${CMAKE_BINARY_DIR}/audio-metrics.json"
    RUN_SERIAL TRUE
    TIMEOUT 600
)
