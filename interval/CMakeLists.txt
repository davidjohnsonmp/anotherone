cmake_minimum_required(VERSION 3.22)
project(interval VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(APPLE)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS")
endif()

option(HICCUP_BUILD_TESTS "Build the offline test programs instead of the plugin" OFF)

# JUCE is downloaded at configure time so the repository itself stays small. Pinned for reproducible builds.
include(FetchContent)
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.16-security
    GIT_SHALLOW TRUE
    GIT_PROGRESS TRUE)
FetchContent_MakeAvailable(JUCE)

file(GLOB HICCUP_IMAGES CONFIGURE_DEPENDS source/assets/img/*.png source/assets/img/*.jpg)
juce_add_binary_data(hiccup_assets SOURCES ${HICCUP_IMAGES}
    source/assets/fonts/Nunito-500.ttf
    source/assets/fonts/Nunito-800.ttf)
set_target_properties(hiccup_assets PROPERTIES POSITION_INDEPENDENT_CODE TRUE)

set(HICCUP_DSP_SOURCES source/PluginProcessor.cpp)
set(HICCUP_GUI_SOURCES source/gui/PluginEditor.cpp)
set(HICCUP_INCLUDES source libs/signalsmith-stretch libs/signalsmith-linear/include libs/signalsmith-linear)
set(HICCUP_DEFS
    JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

if(HICCUP_BUILD_TESTS)
    juce_add_console_app(hiccup_tests PRODUCT_NAME "hiccup_tests")
    juce_generate_juce_header(hiccup_tests)
    target_sources(hiccup_tests PRIVATE tests/hiccup_tests.cpp ${HICCUP_DSP_SOURCES} ${HICCUP_GUI_SOURCES})
    target_include_directories(hiccup_tests PRIVATE ${HICCUP_INCLUDES})
    target_compile_definitions(hiccup_tests PRIVATE ${HICCUP_DEFS} JUCE_ALSA=0 JUCE_JACK=0
        JUCE_USE_XRANDR=0 JUCE_USE_XINERAMA=0 JUCE_USE_XCURSOR=0 JucePlugin_Name="hiccup"
        JucePlugin_Build_Standalone=0)
    target_link_libraries(hiccup_tests PRIVATE hiccup_assets juce::juce_audio_processors juce::juce_audio_utils
        juce::juce_recommended_config_flags juce::juce_recommended_lto_flags)
else()
    # The AU codes below are unchanged from the first build, so existing sessions keep loading.
    juce_add_plugin(interval
        COMPANY_NAME "hiccup audio"
        BUNDLE_ID com.hiccupaudio.interval
        PLUGIN_MANUFACTURER_CODE Hcup
        PLUGIN_CODE Hcp1
        FORMATS AU Standalone
        PRODUCT_NAME "Interval"
        AU_MAIN_TYPE kAudioUnitType_Effect
        IS_SYNTH FALSE
        NEEDS_MIDI_INPUT FALSE
        NEEDS_MIDI_OUTPUT FALSE
        IS_MIDI_EFFECT FALSE
        COPY_PLUGIN_AFTER_BUILD TRUE)
    juce_generate_juce_header(interval)
    target_sources(interval PRIVATE ${HICCUP_DSP_SOURCES} ${HICCUP_GUI_SOURCES})
    target_include_directories(interval PRIVATE ${HICCUP_INCLUDES})
    target_compile_definitions(interval PUBLIC ${HICCUP_DEFS})
    target_link_libraries(interval PRIVATE hiccup_assets juce::juce_audio_processors juce::juce_audio_utils
        PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_warning_flags juce::juce_recommended_lto_flags)
endif()
