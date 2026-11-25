cmake_minimum_required(VERSION 3.22)

project(HoarderLink VERSION 0.1.0)

if(POLICY CMP0135)
  cmake_policy(SET CMP0135 NEW)
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
FetchContent_Declare(
  JUCE
  URL https://github.com/juce-framework/JUCE/archive/refs/tags/9.0.5.tar.gz
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
FetchContent_MakeAvailable(JUCE)

if(WIN32)
  set(HOARDER_VST3_DIR "$ENV{LOCALAPPDATA}/Programs/Common/VST3")
else()
  set(HOARDER_VST3_DIR "$ENV{HOME}/Library/Audio/Plug-Ins/VST3")
endif()
file(MAKE_DIRECTORY "${HOARDER_VST3_DIR}")

juce_add_plugin(HoarderLink
  COMPANY_NAME "Hoarder"
  BUNDLE_ID com.hoarder.link
  IS_SYNTH TRUE
  NEEDS_MIDI_INPUT TRUE
  NEEDS_MIDI_OUTPUT FALSE
  IS_MIDI_EFFECT FALSE
  EDITOR_WANTS_KEYBOARD_FOCUS FALSE
  COPY_PLUGIN_AFTER_BUILD TRUE
  VST3_COPY_DIR "${HOARDER_VST3_DIR}"
  PLUGIN_MANUFACTURER_CODE Hord
  PLUGIN_CODE Hlnk
  FORMATS VST3
  PRODUCT_NAME "Hoarder Link"
  NEEDS_WEB_BROWSER FALSE
  NEEDS_CURL FALSE)

target_sources(HoarderLink
  PRIVATE
    src/LinkClient.cpp
    src/LinkClient.h
    src/PreviewPlayer.cpp
    src/PreviewPlayer.h
    src/PluginEditor.cpp
    src/PluginEditor.h
    src/PluginProcessor.cpp
    src/PluginProcessor.h)

target_compile_definitions(HoarderLink
  PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(HoarderLink
  PRIVATE
    juce::juce_audio_utils
  PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags)
