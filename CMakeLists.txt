# OpenSession - a recording-first multitrack DAW.
# Copyright (C) 2026 The OpenSession contributors.
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Built on JUCE (AGPLv3 option) and Tracktion Engine (GPLv3 option); see
# THIRD_PARTY_NOTICES.md for the exact pinned revisions and licence texts.

cmake_minimum_required(VERSION 3.22)

project(OpenSession
        VERSION 0.1.0
        DESCRIPTION "Recording-first multitrack DAW"
        HOMEPAGE_URL "https://github.com/biggg-nasty/OpenSession"
        LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(APPLE AND NOT CMAKE_OSX_DEPLOYMENT_TARGET)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "12.0" CACHE STRING "Minimum macOS version" FORCE)
endif()

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
endif()

option(OPENSESSION_BUILD_TESTS "Compile the self-test suite into the app (run with --run-tests)" ON)

# ---------------------------------------------------------------------------
# Third-party: Tracktion Engine (which brings its pinned JUCE submodule).
# We disable the engine's example targets; we only want the modules.
# ---------------------------------------------------------------------------
set(TE_ADD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(TRACKTION_ENGINE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/tracktion_engine")

if(NOT EXISTS "${TRACKTION_ENGINE_DIR}/modules/juce/CMakeLists.txt")
    message(FATAL_ERROR
        "JUCE submodule is missing. Run:\n"
        "  git -c url.https://github.com/.insteadOf=git@github.com: submodule update --init --recursive\n"
        "(the engine pins JUCE through an SSH URL; the -c flag rewrites it to HTTPS).")
endif()

add_subdirectory("${TRACKTION_ENGINE_DIR}" EXCLUDE_FROM_ALL)

# ---------------------------------------------------------------------------
# Application
# ---------------------------------------------------------------------------
juce_add_gui_app(OpenSession
    PRODUCT_NAME "OpenSession"
    COMPANY_NAME "OpenSession contributors"
    BUNDLE_ID "org.opensession.daw"
    VERSION ${PROJECT_VERSION}
    MICROPHONE_PERMISSION_ENABLED TRUE
    MICROPHONE_PERMISSION_TEXT "OpenSession records audio from your microphone or audio interface."
    DOCUMENT_EXTENSIONS "osession"
    NEEDS_CURL FALSE
    NEEDS_WEB_BROWSER FALSE)

juce_generate_juce_header(OpenSession)

set(OPENSESSION_SOURCES
    Source/Main.cpp
    Source/App/MainWindow.h
    Source/App/MainComponent.h
    Source/App/MainComponent.cpp
    Source/App/CommandLine.h
    Source/App/CommandLine.cpp
    Source/Engine/OpenSessionEngine.h
    Source/Engine/OpenSessionEngine.cpp
    Source/Engine/DeviceSetup.h
    Source/Engine/DeviceSetup.cpp
    Source/Engine/Recording.h
    Source/Engine/Recording.cpp
    Source/Engine/PluginHelpers.h
    Source/Engine/LatencyCalibration.h
    Source/Engine/LatencyCalibration.cpp
    Source/Session/SessionDocument.h
    Source/Session/SessionDocument.cpp
    Source/Session/Exporter.h
    Source/Session/Exporter.cpp
    Source/Session/WavRepair.h
    Source/Session/WavRepair.cpp
    Source/Session/DemoContent.h
    Source/Session/DemoContent.cpp
    Source/UI/Theme.h
    Source/UI/SessionContext.h
    Source/UI/TransportBar.h
    Source/UI/TransportBar.cpp
    Source/UI/TrackHeader.h
    Source/UI/TrackHeader.cpp
    Source/UI/Timeline.h
    Source/UI/Timeline.cpp
    Source/UI/ClipComponent.h
    Source/UI/ClipComponent.cpp
    Source/UI/Mixer.h
    Source/UI/Mixer.cpp
    Source/UI/DeviceSettingsPanel.h
    Source/UI/DeviceSettingsPanel.cpp
    Source/UI/Meters.h
    Source/UI/StatusBar.h)

if(OPENSESSION_BUILD_TESTS)
    list(APPEND OPENSESSION_SOURCES
        Source/Tests/TestMain.h
        Source/Tests/TestMain.cpp
        Source/Tests/TestHelpers.h
        Source/Tests/RecordingTests.cpp
        Source/Tests/EditingTests.cpp
        Source/Tests/MixExportTests.cpp
        Source/Tests/SessionTests.cpp)
endif()

target_sources(OpenSession PRIVATE ${OPENSESSION_SOURCES})
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} PREFIX Source FILES ${OPENSESSION_SOURCES})

target_include_directories(OpenSession PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/Source"
    "${TRACKTION_ENGINE_DIR}/modules/3rd_party")   # doctest for the self-test suite

target_compile_definitions(OpenSession PRIVATE
    JUCE_APPLICATION_NAME_STRING="$<TARGET_PROPERTY:OpenSession,JUCE_PRODUCT_NAME>"
    JUCE_APPLICATION_VERSION_STRING="$<TARGET_PROPERTY:OpenSession,JUCE_VERSION>"
    JUCE_PLUGINHOST_AU=0
    JUCE_PLUGINHOST_VST3=0
    JUCE_PLUGINHOST_LADSPA=0
    JUCE_MODAL_LOOPS_PERMITTED=1
    JUCE_STRICT_REFCOUNTEDPOINTER=1
    JUCE_USE_CURL=0
    JUCE_WEB_BROWSER=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
    JUCE_REPORT_APP_USAGE=0
    TRACKTION_ENABLE_TIMESTRETCH_SOUNDTOUCH=0
    TRACKTION_UNIT_TESTS=0
    TRACKTION_LOG_DEVICES=1
    OPENSESSION_BUILD_TESTS=$<BOOL:${OPENSESSION_BUILD_TESTS}>
    OPENSESSION_HOMEPAGE_URL="${PROJECT_HOMEPAGE_URL}")

target_link_libraries(OpenSession PRIVATE
    tracktion::tracktion_core
    tracktion::tracktion_engine
    tracktion::tracktion_graph
    juce::juce_audio_devices
    juce::juce_audio_formats
    juce::juce_audio_processors
    juce::juce_audio_utils
    juce::juce_gui_extra
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags)

if(APPLE)
    # Same workaround the engine's own examples use with current Apple clang.
    target_compile_options(OpenSession PRIVATE "-fno-aligned-allocation")
    set_source_files_properties("${TRACKTION_ENGINE_DIR}/modules/juce/modules/juce_events/juce_events.mm"
        DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        PROPERTIES COMPILE_OPTIONS "-Wno-implicit-int-float-conversion")
    set_source_files_properties("${TRACKTION_ENGINE_DIR}/modules/juce/modules/juce_audio_processors_headless/juce_audio_processors_headless.mm"
        DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        PROPERTIES COMPILE_OPTIONS "-Wno-deprecated-declarations")
endif()

# Record the engine revision we were built against so the app can display it.
execute_process(COMMAND git -C "${TRACKTION_ENGINE_DIR}" rev-parse --short HEAD
                OUTPUT_VARIABLE OPENSESSION_ENGINE_REVISION OUTPUT_STRIP_TRAILING_WHITESPACE
                ERROR_QUIET)
target_compile_definitions(OpenSession PRIVATE OPENSESSION_ENGINE_REVISION="${OPENSESSION_ENGINE_REVISION}")
