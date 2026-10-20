cmake_minimum_required(VERSION 3.22)

if(NOT DEFINED CMAKE_OSX_DEPLOYMENT_TARGET OR CMAKE_OSX_DEPLOYMENT_TARGET STREQUAL "")
    set(CMAKE_OSX_DEPLOYMENT_TARGET "10.15" CACHE STRING "Minimum macOS version" FORCE)
endif()

project(CyberUltimateHybridVST VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.4
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(JUCE)

set(PLUGIN_FORMATS VST3 Standalone LV2)
if(APPLE)
    list(APPEND PLUGIN_FORMATS AU)
endif()

juce_add_plugin(CyberUltimateHybrid
    COMPANY_NAME "Cyber Audio"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    PLUGIN_MANUFACTURER_CODE CyAu
    PLUGIN_CODE CyUH
    FORMATS ${PLUGIN_FORMATS}
    LV2URI "http://cyber-audio.co.uk/plugins/cyber-ultimate-hybrid-vst"
    PRODUCT_NAME "Cyber Ultimate Hybrid"
)

target_sources(CyberUltimateHybrid
    PRIVATE
        src/PluginProcessor.cpp
        src/PluginProcessor.h
        src/PluginEditor.cpp
        src/PluginEditor.h
)

target_include_directories(CyberUltimateHybrid
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}/../cyber-ultimate-hybrid.lv2/src
)

target_compile_definitions(CyberUltimateHybrid
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
        CYBER_ENABLE_STUDIO_DELAY=1
        $<$<PLATFORM_ID:Windows>:JUCE_DIRECTSOUND=1>
        $<$<PLATFORM_ID:Windows>:JUCE_WASAPI=1>
)

target_link_libraries(CyberUltimateHybrid
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)
