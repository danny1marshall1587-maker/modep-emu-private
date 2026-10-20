cmake_minimum_required(VERSION 3.15)

if(APPLE)
    set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "" FORCE)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "" FORCE)
endif()

project(GALAXY_STROBE_TUNE VERSION 2.0.0 LANGUAGES C CXX)

include(FetchContent)
FetchContent_Declare(
    juce
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.12
)
FetchContent_MakeAvailable(juce)

juce_add_plugin(GalaxyStrobeTune
    PRODUCT_NAME "Galaxy Strobe Tune"
    COMPANY_NAME "Danny Marshall"
    BUNDLE_ID com.dannymarshall.strobetune
    PLUGIN_MANUFACTURER_CODE DANM
    PLUGIN_CODE Strb
    FORMATS VST3 AU Standalone
    COPY_PLUGIN_AFTER_BUILD TRUE
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
)

target_sources(GalaxyStrobeTune
    PRIVATE
        Source/PluginProcessor.cpp
        Source/PluginProcessor.h
        Source/PluginEditor.cpp
        Source/PluginEditor.h
)

target_compile_definitions(GalaxyStrobeTune
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(GalaxyStrobeTune
    PRIVATE
        juce::juce_audio_utils
        juce::juce_audio_processors
        juce::juce_gui_extra
        juce::juce_opengl
        juce::juce_dsp
)

juce_generate_juce_header(GalaxyStrobeTune)
