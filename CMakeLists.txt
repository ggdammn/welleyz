cmake_minimum_required(VERSION 3.22)
project(Welleyz VERSION 0.4.0)

set(WELLEYZ_UPDATE_URL "" CACHE STRING
    "GitHub Releases latest endpoint")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
FetchContent_Declare(
  JUCE
  GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
  GIT_TAG 8.0.8
  GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(JUCE)

juce_add_plugin(Welleyz
  COMPANY_NAME "shadowave"
  COMPANY_WEBSITE "https://github.com/ggdammn/welleyz.vst"
  BUNDLE_ID "com.shadowave.welleyz"
  PLUGIN_NAME "welleyz"
  DESCRIPTION "welleyz by shadowave — generative synthesizer with 3 oscillators and FX."
  PLUGIN_MANUFACTURER_CODE ShWv
  PLUGIN_CODE Wlyz
  FORMATS VST3 Standalone
  PRODUCT_NAME "welleyz"
  ICON_BIG "${CMAKE_CURRENT_SOURCE_DIR}/src/assets/standalone-icon.png"
  ICON_SMALL "${CMAKE_CURRENT_SOURCE_DIR}/src/assets/standalone-icon.png"
  IS_SYNTH TRUE
  NEEDS_MIDI_INPUT TRUE
  EDITOR_WANTS_KEYBOARD_FOCUS TRUE
  NEEDS_MIDI_OUTPUT FALSE
  IS_MIDI_EFFECT FALSE
  COPY_PLUGIN_AFTER_BUILD FALSE
  VST3_CATEGORIES "Instrument;Synth;Chiptune"
)

juce_add_binary_data(WelleyzAssets
  HEADER_NAME WelleyzAssets.h
  NAMESPACE WelleyzAssets
  SOURCES
    src/assets/branding/logo.svg
    src/assets/fonts/Syne-variable.ttf
    src/assets/fonts/Outfit-variable.ttf
    src/assets/fonts/Sarina-Regular.ttf
    src/resources/1bass.wlyz
    src/resources/1bell.wlyz
    src/resources/1lead.wlyz
    src/resources/1pad.wlyz
    src/resources/2pad.wlyz
    src/resources/arpsynth.wlyz
    src/resources/detunedkeys.wlyz
    src/resources/foggypad.wlyz
    src/resources/noisypad.wlyz
    src/resources/organ.wlyz
    src/resources/Manual.md
    "${CMAKE_CURRENT_SOURCE_DIR}/src/assets/icons/back icon.svg"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/assets/icons/colors icon.svg"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/assets/icons/fx icon.svg"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/assets/icons/like preset icon.svg"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/assets/icons/load icon.svg"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/assets/icons/manual_icon.svg"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/assets/icons/metronome_icon.svg"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/assets/icons/osc icon.svg"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/assets/icons/random icon.svg"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/assets/icons/record_icon.svg"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/assets/icons/reset icon.svg"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/assets/icons/save  icon.svg"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/assets/icons/scale icon.svg"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/assets/icons/settings icon.svg"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/assets/icons/soundtypes icons.svg"
)

juce_generate_juce_header(Welleyz)

target_sources(Welleyz PRIVATE
  src/PluginProcessor.cpp
  src/PluginEditor.cpp
  src/UpdateChecker.cpp
)

target_compile_definitions(Welleyz PRIVATE
  JUCE_WEB_BROWSER=0
  JUCE_USE_CURL=0
  JUCE_VST3_CAN_REPLACE_VST2=0
  WELLEYZ_VERSION="${PROJECT_VERSION}"
  WELLEYZ_UPDATE_URL="${WELLEYZ_UPDATE_URL}"
)

target_link_libraries(Welleyz PRIVATE
  WelleyzAssets
  juce::juce_audio_utils
  juce::juce_audio_processors
  juce::juce_audio_basics
  juce::juce_audio_devices
  juce::juce_audio_formats
  juce::juce_dsp
  juce::juce_graphics
  juce::juce_gui_basics
  juce::juce_gui_extra
  juce::juce_data_structures
  juce::juce_core
)

juce_add_console_app(PresetCompatibility PRODUCT_NAME PresetCompatibility)
target_sources(PresetCompatibility PRIVATE tests/PresetCompatibility.cpp)
target_link_libraries(PresetCompatibility PRIVATE WelleyzAssets juce::juce_core juce::juce_data_structures)

juce_add_console_app(FxEngineTest PRODUCT_NAME FxEngineTest)
juce_generate_juce_header(FxEngineTest)
target_sources(FxEngineTest PRIVATE tests/FxEngineTest.cpp)
target_include_directories(FxEngineTest PRIVATE src)
target_link_libraries(FxEngineTest PRIVATE juce::juce_audio_processors juce::juce_dsp)

juce_add_console_app(ProcessorBehaviourTest PRODUCT_NAME ProcessorBehaviourTest)
juce_generate_juce_header(ProcessorBehaviourTest)
target_sources(ProcessorBehaviourTest PRIVATE tests/ProcessorBehaviourTest.cpp src/PluginProcessor.cpp)
target_include_directories(ProcessorBehaviourTest PRIVATE src)
target_link_libraries(ProcessorBehaviourTest PRIVATE WelleyzAssets juce::juce_audio_utils juce::juce_dsp)

juce_add_console_app(EditorSmokeTest PRODUCT_NAME EditorSmokeTest)
juce_generate_juce_header(EditorSmokeTest)
target_sources(EditorSmokeTest PRIVATE tests/EditorSmokeTest.cpp src/PluginProcessor.cpp src/PluginEditor.cpp src/UpdateChecker.cpp)
target_include_directories(EditorSmokeTest PRIVATE src)
target_link_libraries(EditorSmokeTest PRIVATE WelleyzAssets juce::juce_audio_utils juce::juce_dsp)

juce_add_console_app(StandaloneTransportTest PRODUCT_NAME StandaloneTransportTest)
juce_generate_juce_header(StandaloneTransportTest)
target_sources(StandaloneTransportTest PRIVATE tests/StandaloneTransportTest.cpp src/PluginProcessor.cpp src/PluginEditor.cpp src/UpdateChecker.cpp)
target_include_directories(StandaloneTransportTest PRIVATE src)
target_link_libraries(StandaloneTransportTest PRIVATE WelleyzAssets juce::juce_audio_utils juce::juce_dsp)

if(WIN32)
  configure_file(packaging/WelleyzUninstall.rc.in
                 "${CMAKE_CURRENT_BINARY_DIR}/welleyz_uninstall.rc" @ONLY)
  add_executable(welleyz_uninstall WIN32
    packaging/WelleyzUninstall.cpp
    "${CMAKE_CURRENT_BINARY_DIR}/welleyz_uninstall.rc")
  set_target_properties(welleyz_uninstall PROPERTIES OUTPUT_NAME "welleyz-uninstall")
  target_compile_definitions(welleyz_uninstall PRIVATE UNICODE _UNICODE)
  target_link_libraries(welleyz_uninstall PRIVATE shell32 advapi32 user32)

  configure_file(packaging/WelleyzUpdate.rc.in
                 "${CMAKE_CURRENT_BINARY_DIR}/welleyz_update.rc" @ONLY)
  add_executable(welleyz_update WIN32
    packaging/WelleyzUpdate.cpp
    "${CMAKE_CURRENT_BINARY_DIR}/welleyz_update.rc")
  set_target_properties(welleyz_update PROPERTIES OUTPUT_NAME "welleyz-update")
  target_compile_definitions(welleyz_update PRIVATE UNICODE _UNICODE
    WELLEYZ_VERSION="${PROJECT_VERSION}"
    WELLEYZ_UPDATE_URL="${WELLEYZ_UPDATE_URL}")
  target_link_libraries(welleyz_update PRIVATE winhttp shell32 advapi32 user32)
endif()
