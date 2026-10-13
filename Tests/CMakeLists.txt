set(PROFILER_TEST_SOURCES
    TestRunner.cpp
    AudioProcessorTests.cpp
    ProfileProcessorIntegrationTests.cpp
    ProfileManagerTests.cpp
    LoggingTests.cpp
    NotificationBannerTests.cpp
    UiSettingsTests.cpp
    MarketplaceClientTests.cpp
    SpectraEngineTests.cpp
)

add_executable(ProfilerTests EXCLUDE_FROM_ALL
    ${PROFILER_TEST_SOURCES}
    ../Source/Src/Components/NotificationBanner.cpp
    ../Source/Src/Logging.cpp
    ../Source/Src/PluginProcessor.cpp
    ../Source/Src/Fx/FxCatalog.cpp
    ../Source/Src/Fx/FxDsp.cpp
    ../Source/Src/Fx/FxRack.cpp
    ../Source/Src/ProfileManager.cpp
    ../Source/Src/UiSettings.cpp
    ../Source/Src/MarketplaceClient.cpp
    ../Source/Src/SpectraEngine.cpp
    ../Source/Src/SpectraAmp.cpp
)

target_include_directories(ProfilerTests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/Support
    ${CMAKE_SOURCE_DIR}/Source/Include
)

target_compile_definitions(ProfilerTests PRIVATE
    JUCE_UNIT_TESTS=1
    JUCE_DISPLAY_SPLASH_SCREEN=0
    JUCE_REPORT_APP_USAGE=0
    PROFILER_HEADLESS_TESTS=1
    JucePlugin_IsSynth=0
    JucePlugin_WantsMidiInput=0
    JucePlugin_ProducesMidiOutput=0
    JucePlugin_IsMidiEffect=0
    JucePlugin_Name="Profiler"
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
            COMMAND "$<TARGET_FILE:ProfilerTests>" --quiet
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
            COMMAND "${CMAKE_CTEST_COMMAND}" --test-dir "${CMAKE_BINARY_DIR}" --output-on-failure
            COMMAND "$<TARGET_FILE:ProfilerTests>" --quiet
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

function(add_profiler_profile_manager_test test_name case_name)
    add_test(NAME ProfilerTests.ProfileManager.${test_name}
        COMMAND ProfilerTests --case "${case_name}")
endfunction()

add_profiler_profile_manager_test(CreateProfile "create profile")
add_profiler_profile_manager_test(UpdateProfile "update profile")
add_profiler_profile_manager_test(DeleteProfile "delete profile")
add_profiler_profile_manager_test(ExportProfile "export profile")
add_profiler_profile_manager_test(ImportProfile "import profile")
add_profiler_profile_manager_test(RejectOutOfRangeNumericValue "reject out-of-range numeric value")
add_profiler_profile_manager_test(RejectNonStringPathValue "reject non-string path value")
add_profiler_profile_manager_test(RejectInvalidProfileJson "reject invalid profile JSON")
add_profiler_profile_manager_test(RejectUnsupportedProfileFormat "reject unsupported profile format")
add_profiler_profile_manager_test(ApplyParameterMapping "apply profile values to APVTS parameters")
add_profiler_profile_manager_test(SaveCurrentProfileSetting "save current profile setting")
add_profiler_profile_manager_test(ClearCurrentProfileSetting "clear current profile setting")
add_profiler_profile_manager_test(CaptureCurrentValues "capture current APVTS values with profile file paths")

function(add_profiler_audio_processor_test test_name case_name)
    add_test(NAME ProfilerTests.AudioProcessor.${test_name}
        COMMAND ProfilerTests --case "${case_name}")
endfunction()

add_profiler_audio_processor_test(Metadata "audio processor metadata")
add_profiler_audio_processor_test(BusLayouts "audio processor bus layouts")
add_profiler_audio_processor_test(ParameterDefaults "audio processor parameter defaults")
add_profiler_audio_processor_test(StateRoundTrip "audio processor state round trip")
add_profiler_audio_processor_test(ChainLayoutRoundTrip "audio processor chain layout round trip")
add_profiler_audio_processor_test(ProcessBlockSmoke "audio processor process block smoke")
add_profiler_audio_processor_test(MuteClearsBuffer "audio processor mute clears buffer")
add_profiler_audio_processor_test(MasterZeroSilencesBuffer "audio processor master zero silences buffer")
add_profiler_audio_processor_test(NoiseGateZeroBypasses "audio processor noise gate zero bypasses")
add_profiler_audio_processor_test(GainChangesOutputLevel "audio processor gain changes output level")
add_profiler_audio_processor_test(SilentInputStaysSilent "audio processor silent input stays silent")
add_profiler_audio_processor_test(PrepareBlockSizes "audio processor handles prepare block sizes")
add_profiler_audio_processor_test(ReleaseAndReprepare "audio processor release and reprepare")
add_profiler_audio_processor_test(RejectMissingAssetFiles "audio processor rejects missing asset files")
add_profiler_audio_processor_test(AmpModelResidualAndBypass "audio processor amp model residual and bypass")
add_profiler_audio_processor_test(SpectraNeuralAmp "audio processor spectra neural amp")
add_profiler_audio_processor_test(EffectChain "audio processor effect chain")

function(add_profiler_profile_processor_integration_test test_name case_name)
    add_test(NAME ProfilerTests.ProfileProcessorIntegration.${test_name}
        COMMAND ProfilerTests --case "${case_name}")
endfunction()

add_profiler_profile_processor_integration_test(ApplyMapsProfileValues "profile processor apply maps profile values")
add_profiler_profile_processor_integration_test(TracksProfileAssetPaths "profile processor tracks profile asset paths")
add_profiler_profile_processor_integration_test(AppliedProfileAffectsAudio "profile processor applied profile affects audio")
add_profiler_profile_processor_integration_test(SyncsCurrentProfileSelection "profile processor syncs current profile selection")

function(add_profiler_logging_test test_name case_name)
    add_test(NAME ProfilerTests.Logging.${test_name}
        COMMAND ProfilerTests --case "${case_name}")
endfunction()

add_profiler_logging_test(CreateAndFind "create and find logger")
add_profiler_logging_test(DuplicateCreateThrows "duplicate create throws")
add_profiler_logging_test(GetMissingThrows "get non-existent throws")
add_profiler_logging_test(FormatLabels "format log labels")
add_profiler_logging_test(ConfigAndFileOutput "parse config and write log file")

function(add_profiler_ui_settings_test test_name case_name)
    add_test(NAME ProfilerTests.UiSettings.${test_name}
        COMMAND ProfilerTests --case "${case_name}")
endfunction()

add_profiler_ui_settings_test(DefaultsAndNormalisation "ui settings defaults and normalisation")
add_profiler_ui_settings_test(Persistence "ui settings persistence")
add_profiler_ui_settings_test(InvalidJson "ui settings invalid JSON")

function(add_profiler_notification_banner_test test_name case_name)
    add_test(NAME ProfilerTests.NotificationBanner.${test_name}
        COMMAND ProfilerTests --case "${case_name}")
endfunction()

add_profiler_notification_banner_test(Lifecycle "notification banner lifecycle")
add_profiler_notification_banner_test(Painting "notification banner painting")

function(add_profiler_marketplace_test test_name case_name)
    add_test(NAME ProfilerTests.Marketplace.${test_name}
        COMMAND ProfilerTests --case "${case_name}")
endfunction()

add_profiler_marketplace_test(Login "marketplace login")
add_profiler_marketplace_test(Mfa "marketplace mfa")
add_profiler_marketplace_test(Library "marketplace library")
add_profiler_marketplace_test(Session "marketplace session file")
add_profiler_marketplace_test(IrCapture "marketplace ir capture")
