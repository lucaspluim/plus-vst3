cmake_minimum_required(VERSION 3.22)

project(AudioVisualizer VERSION 0.1.0)

# JUCE setup
set(JUCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../JUCE")
add_subdirectory(${JUCE_DIR} ${CMAKE_CURRENT_BINARY_DIR}/JUCE)

# Create the plugin
juce_add_plugin(AudioVisualizer
    COMPANY_NAME "YourCompany"
    PLUGIN_MANUFACTURER_CODE Yoco
    PLUGIN_CODE Avis
    FORMATS Standalone VST3 AU
    PRODUCT_NAME "+"

    # Plugin characteristics
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE

    # Standalone settings - no input to avoid "muted" warning
    PLUGIN_MANUFACTURER_CODE Yoco
    PLUGIN_CODE Avis

    # Copy to standard plugin locations
    COPY_PLUGIN_AFTER_BUILD TRUE
)

# Source files
target_sources(AudioVisualizer
    PRIVATE
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp
        Source/StarfieldInstanceImpl.cpp
        Source/EffectSystem.h
        Source/EffectBox.h
)

# Compile definitions
target_compile_definitions(AudioVisualizer
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
)

# Link JUCE modules
target_link_libraries(AudioVisualizer
    PRIVATE
        juce::juce_audio_utils
        juce::juce_audio_processors
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)
