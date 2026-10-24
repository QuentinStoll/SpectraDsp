# Plugin compiling

set(PLUGIN_NAME Profiler)

set(PROFILER_PLUGIN_FORMATS)
if(PLUGIN_AU)
    list(APPEND PROFILER_PLUGIN_FORMATS AU)
endif()
if(PLUGIN_VST3)
    list(APPEND PROFILER_PLUGIN_FORMATS VST3)
endif()
if(PLUGIN_STANDALONE)
    list(APPEND PROFILER_PLUGIN_FORMATS Standalone)
endif()

if(NOT PROFILER_PLUGIN_FORMATS)
    list(APPEND PROFILER_PLUGIN_FORMATS VST3 Standalone)
endif()

juce_add_plugin(${PLUGIN_NAME}
    COMPANY_NAME "Profiler"
    PRODUCT_NAME "Profiler"
    PLUGIN_NAME "Profiler"

    FORMATS ${PROFILER_PLUGIN_FORMATS}
    ICON_BIG   "${CMAKE_CURRENT_SOURCE_DIR}/Assets/png/ProfilerPP.png"
    ICON_SMALL "${CMAKE_CURRENT_SOURCE_DIR}/Assets/png/ProfilerPP.png"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
)

set_target_properties(${PLUGIN_NAME}  PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/arc"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/lib"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

juce_add_binary_data(ProfilerAssets
    SOURCES
        Assets/Svg/cross-icon.svg
        Assets/Svg/music-note.svg
        Assets/Svg/plus-icon.svg
        Assets/Fonts/Orbitron-Regular.ttf
        Assets/Fonts/Orbitron-Medium.ttf
        Assets/Fonts/Orbitron-Bold.ttf
        Assets/Fonts/Orbitron-ExtraBold.ttf
        Assets/Fonts/Orbitron-Black.ttf
)

# Collect source files
target_sources(${PLUGIN_NAME} PRIVATE
    Src/Components/CustomComboBox.cpp
    Src/Components/CustomImageButton.cpp
    Src/Components/CustomKnob.cpp
    Src/Components/CustomLookAndFeel.cpp
    Src/Components/CustomLinearSlider.cpp
    Src/Components/CustomTabBar.cpp
    Src/Components/CustomTabs.cpp
    Src/Components/CustomTextButton.cpp
    Src/Components/CustomTextEditor.cpp
    Src/Components/CustomToggleButton.cpp
    Src/Components/FileDropZone.cpp
    Src/Components/BlockPickerPage.cpp
    Src/Components/SignalChainStrip.cpp
    Src/Components/FormFields/FormComboField.cpp
    Src/Components/FormComponent.cpp
    Src/Components/FormFields/FormField.cpp
    Src/Components/FormFields/FormPathField.cpp
    Src/Components/FormFields/FormSliderField.cpp
    Src/Components/FormFields/FormTextField.cpp
    Src/Components/FormFields/FormToggleField.cpp
    Src/Components/FileStatusCard.cpp
    Src/Components/ModalOverlay.cpp
    Src/Components/NotificationBanner.cpp
    Src/Modules/AddProfilModule.cpp
    Src/Modules/AdvancedEqModule.cpp
    Src/Modules/BasicEqModule.cpp
    Src/Modules/CardGridModule.cpp
    Src/Modules/CreateProfilModule.cpp
    Src/Modules/DeleteProfileConfirmationModule.cpp
    Src/Modules/EditProfilModule.cpp
    Src/Modules/ExportProfilModule.cpp
    Src/Modules/FxEditPanel.cpp
    Src/Fx/FxCatalog.cpp
    Src/Fx/FxDsp.cpp
    Src/Fx/FxRack.cpp
    Src/Modules/FileAssetsModule.cpp
    Src/Modules/MasterSlidersModule.cpp
    Src/Modules/StudioEditPanels.cpp
    Src/Modules/StudioTopBar.cpp
    Src/Modules/UtilityBarModule.cpp
    Src/AmpCapture.cpp
    Src/MarketplaceClient.cpp
    Src/SpectraEngine.cpp
    Src/SpectraAmp.cpp
    Src/Views/CloneView.cpp
    Src/Views/MarketplaceAccountPanel.cpp
    Src/Views/PlayView.cpp
    Src/Views/ProfilView.cpp
    Src/Views/AmpCapturePanel.cpp
    Src/Views/IrCapturePanel.cpp
    Src/Views/SettingsView.cpp
    Src/ProfileManager.cpp
    Src/UiSettings.cpp
    Src/PluginEditor.cpp
    Src/PluginProcessor.cpp
    Src/Logging.cpp
)

# Create plugin library
# add_library(${PLUGIN_NAME} STATIC ${SOURCES})
add_compile_definitions(JUCE_VST3_CAN_REPLACE_VST2=0)

# Link JUCE modules from juce_setup.cmake
set(PROFILER_PLATFORM_LIBS)
if(UNIX AND NOT APPLE)
    list(APPEND PROFILER_PLATFORM_LIBS CURL::libcurl)
endif()

target_link_libraries(${PLUGIN_NAME} PRIVATE
    ${JUCE_MODULES_AVAILABLE}
    ProfilerAssets
    simdjson
    RTNeural
    TracyClient
    ${PROFILER_PLATFORM_LIBS}
)

# Generate the aggregate header consumed by the existing source files.  Unlike
# Projucer, the CMake workflow writes it to the target's build artefacts.
juce_generate_juce_header(${PLUGIN_NAME})

# Include directories
target_include_directories(${PLUGIN_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/Include
    ${rtneural_SOURCE_DIR}
)

# Apply compiler options and preset-based defines
apply_compiler_options(${PLUGIN_NAME})
profiler_link_nam_engine(${PLUGIN_NAME})
foreach(spectra_nam_format VST3 Standalone AU)
    if(TARGET ${PLUGIN_NAME}_${spectra_nam_format})
        profiler_link_nam_engine(${PLUGIN_NAME}_${spectra_nam_format})
    endif()
endforeach()

profiler_attach_spectra_dsp(${PLUGIN_NAME}_VST3)
profiler_attach_spectra_dsp(${PLUGIN_NAME}_Standalone)
profiler_attach_spectra_dsp(${PLUGIN_NAME}_AU)

