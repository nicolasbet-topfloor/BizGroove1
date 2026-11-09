cmake_minimum_required(VERSION 3.22)

# These must come before project(): afterwards they have no effect.
# 10.13 High Sierra is the oldest macOS that current Xcode can build for.
set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING "Minimum macOS version")
set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "Universal binary on macOS")

project(BizGroove VERSION 1.0.0 LANGUAGES C CXX)

# Windows: build the C++ runtime into BizGroove.exe, so it runs on any PC
# without installing the Visual C++ Redistributable.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# JUCE 8 is fetched automatically. To use a local copy instead:
#   cmake -B build -DFETCHCONTENT_SOURCE_DIR_JUCE=/path/to/JUCE
include(FetchContent)
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.4
    GIT_SHALLOW ON)
FetchContent_MakeAvailable(JUCE)

# ---------------------------------------------------------------- engine
# The audio engine has no GUI dependency, so the app and the tests compile
# the same sources. (JUCE modules must be compiled once per target.)
set(ENGINE_SOURCES
    Source/engine/AudioEngine.cpp
    Source/engine/StemDeck.cpp
    Source/engine/Song.cpp
    Source/engine/DrumKit.cpp
    Source/engine/Mixer.cpp
    Source/engine/Sampler.cpp
    Source/engine/Project.cpp
    Source/engine/ControlTargets.cpp
    Source/engine/ControllerProfile.cpp
    Source/engine/MidiRouter.cpp)
set(COMMON_DEFS JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 BIZGROOVE_VERSION="${PROJECT_VERSION}")

# ---------------------------------------------------------------- app
juce_add_gui_app(BizGroove
    PRODUCT_NAME "BizGroove"
    COMPANY_NAME "BizGroove"
    BUNDLE_ID "org.bizgroove.app"
    VERSION ${PROJECT_VERSION}
    MICROPHONE_PERMISSION_ENABLED TRUE
    MICROPHONE_PERMISSION_TEXT "BizGroove uses audio input for recording audio tracks.")

target_sources(BizGroove PRIVATE
    ${ENGINE_SOURCES}
    Source/Main.cpp
    Source/ui/MainComponent.cpp
    Source/ui/StepGrid.cpp
    Source/ui/LookAndFeel.cpp
    Source/ui/StemPanel.cpp
    Source/ui/LaunchkeyPanel.cpp
    Source/ui/SessionBar.cpp
    Source/ui/SongView.cpp
    Source/ui/TimelineView.cpp
    Source/ui/MixerView.cpp
    Source/ui/ExportDialog.cpp
    Source/ui/ControllersWindow.cpp)

target_include_directories(BizGroove PRIVATE Source)
target_compile_definitions(BizGroove PRIVATE ${COMMON_DEFS})
target_link_libraries(BizGroove
    PRIVATE
        juce::juce_gui_extra
        juce::juce_audio_utils
        juce::juce_audio_devices
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# ---------------------------------------------------------------- tests
juce_add_console_app(BizGrooveTests PRODUCT_NAME "BizGrooveTests")
target_sources(BizGrooveTests PRIVATE ${ENGINE_SOURCES} Tests/EngineTests.cpp)
target_include_directories(BizGrooveTests PRIVATE Source)
target_compile_definitions(BizGrooveTests PRIVATE ${COMMON_DEFS})
target_link_libraries(BizGrooveTests
    PRIVATE
        juce::juce_audio_basics
        juce::juce_audio_devices
        juce::juce_audio_formats
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags)

enable_testing()
add_test(NAME EngineTests COMMAND BizGrooveTests)
