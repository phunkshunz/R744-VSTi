cmake_minimum_required(VERSION 3.22)

project(R744 VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Windows: bygg in C++-runtime i pluginet, så att köparen inte behöver installera något extra
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

# JUCE hämtas automatiskt vid bygget
include(FetchContent)
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.4
    GIT_SHALLOW ON)
FetchContent_MakeAvailable(JUCE)

if(APPLE)
    set(R744_FORMATS VST3 AU Standalone)
else()
    set(R744_FORMATS VST3 Standalone)
endif()

juce_add_plugin(R744
    COMPANY_NAME "phunkshunz"
    PRODUCT_NAME "R-744"
    BUNDLE_ID "com.phunkshunz.r744"
    PLUGIN_MANUFACTURER_CODE Phnk
    PLUGIN_CODE R744
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES Instrument Sampler
    AU_MAIN_TYPE kAudioUnitType_MusicDevice
    FORMATS ${R744_FORMATS})

target_sources(R744
    PRIVATE
        src/PluginProcessor.cpp
        src/PluginEditor.cpp)

target_compile_definitions(R744
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(R744
    PRIVATE
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# MSVC: läs källkoden som UTF-8, så att å, ä och ö blir rätt
if(MSVC)
    target_compile_options(R744 PRIVATE /utf-8 /bigobj)
endif()
