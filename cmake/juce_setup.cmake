if(NOT TARGET juce::juce_core)
    message(FATAL_ERROR "JUCE not found. Make sure dependencies.cmake is included first.")
endif()

# Juce Setup
# plugin format check
if(NOT JUCE_SUPPORTS_AAX AND PLUGIN_AAX)
    message(WARNING "AAX requested but not available in this JUCE installation")
    set(PLUGIN_AAX false)
endif()

if(NOT APPLE AND PLUGIN_AU)
    message(STATUS "AU format skipped: it is only available on macOS")
    set(PLUGIN_AU false)
endif()

juce_disable_default_flags()

set(JUCE_MODULES_AVAILABLE
    juce_audio_basics
    juce_audio_devices
    juce_audio_formats
    juce_audio_plugin_client
    juce_audio_processors
    juce_audio_utils
    juce_core
    juce_data_structures
    juce_dsp
    juce_events
    juce_graphics
    juce_gui_basics
    juce_gui_extra
)
