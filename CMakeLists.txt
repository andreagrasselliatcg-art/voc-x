cmake_minimum_required(VERSION 3.22)

if(APPLE)
    set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "" FORCE)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "" FORCE)
endif()

project(VocX VERSION 1.0.0 LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(DEFINED JUCE_DIR)
    add_subdirectory(${JUCE_DIR} ${CMAKE_BINARY_DIR}/JUCE)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 8.0.4
        GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(JUCE)
endif()

set(VX_FORMATS VST3 Standalone)
if(APPLE)
    list(APPEND VX_FORMATS AU)
endif()

juce_add_plugin(VocX
    COMPANY_NAME "Tune Talks"
    BUNDLE_ID "com.tunetalks.vocx"
    PLUGIN_MANUFACTURER_CODE Tntk
    PLUGIN_CODE VocX
    FORMATS ${VX_FORMATS}
    PRODUCT_NAME "Voc-X"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD TRUE
    AU_MAIN_TYPE kAudioUnitType_MusicEffect
    VST3_CATEGORIES Fx
    DESCRIPTION "Multi-mode vocoder with follow-voice carrier, chords and effects")

set(VX_SOURCES Source/PluginProcessor.cpp Source/PluginEditor.cpp)
target_sources(VocX PRIVATE ${VX_SOURCES})
target_include_directories(VocX PRIVATE Source)

target_compile_definitions(VocX PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

set(VX_LINUX_MIN_DEFS JUCE_USE_XRANDR=0 JUCE_USE_XINERAMA=0 JUCE_USE_XCURSOR=0 JUCE_USE_XRENDER=0 JUCE_ALSA=0 JUCE_JACK=0)
if(VX_MINIMAL_LINUX)
    target_compile_definitions(VocX PUBLIC ${VX_LINUX_MIN_DEFS})
endif()

target_link_libraries(VocX
    PRIVATE juce::juce_audio_utils
    PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_lto_flags juce::juce_recommended_warning_flags)

# Developer test harness: cmake -DVX_BUILD_TESTS=ON
if(VX_BUILD_TESTS)
    juce_add_console_app(VocXTests PRODUCT_NAME "VocXTests")
    target_sources(VocXTests PRIVATE Tests/TestMain.cpp ${VX_SOURCES})
    target_include_directories(VocXTests PRIVATE Source)
    target_compile_definitions(VocXTests PRIVATE JucePlugin_Name="Voc-X" JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_STANDALONE_APPLICATION=1 JUCE_MODAL_LOOPS_PERMITTED=1)
    if(VX_MINIMAL_LINUX)
        target_compile_definitions(VocXTests PRIVATE ${VX_LINUX_MIN_DEFS})
    endif()
    target_link_libraries(VocXTests PRIVATE juce::juce_audio_utils PUBLIC juce::juce_recommended_config_flags)
endif()
