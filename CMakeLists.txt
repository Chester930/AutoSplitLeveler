cmake_minimum_required(VERSION 3.15.0)

project(AutoSplitLeveler VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Disable VST3 SDK samples and GUI to speed up build
set(SMTG_ADD_VST3_PLUGINS_SAMPLES OFF CACHE BOOL "Add VST3 Plugins Samples" FORCE)
set(SMTG_ADD_VST3_HOSTING_SAMPLES OFF CACHE BOOL "Add VST3 Hosting Samples" FORCE)
set(SMTG_ADD_VSTGUI OFF CACHE BOOL "Add VSTGUI" FORCE)
set(SMTG_BUILD_DOCS OFF CACHE BOOL "Build Docs" FORCE)

# Path to the VST3 SDK
set(vst3sdk_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/vst3sdk")
add_subdirectory(${vst3sdk_SOURCE_DIR})

# Add ARA SDK
set(ARA_API_DIR "${CMAKE_CURRENT_SOURCE_DIR}/ARA_SDK/ARA_API")
set(ARA_LIBRARY_DIR "${CMAKE_CURRENT_SOURCE_DIR}/ARA_SDK/ARA_Library")
add_subdirectory(${ARA_LIBRARY_DIR})

set(target AutoSplitLeveler)
set(sources
    source/ARA/VST3/TestVST3Entry.cpp
    source/ARA/VST3/TestVST3Processor.cpp
    source/ARA/VST3/TestVST3Processor.h
    source/ARA/ARATestAudioSource.cpp
    source/ARA/ARATestDocumentController.cpp
    source/ARA/ARATestPlaybackRenderer.cpp
    source/ARA/AutoSplitLevelerAlgorithm.cpp
    source/ARA/TestAnalysis.cpp
    source/ARA/TestPersistency.cpp
    ARA_SDK/ARA_Library/Debug/ARADebug.c
    source/silencedetector.cpp
    source/silencedetector.h
    source/zerocrossing.cpp
    source/zerocrossing.h
    source/loudnessanalyzer.cpp
    source/loudnessanalyzer.h
    source/levelingengine.cpp
    source/levelingengine.h
    source/version.h
)

set(SMTG_RUN_VST_VALIDATOR OFF)
set(SMTG_RUN_VST_MODULE_INFO_TOOL OFF)
set(SMTG_CREATE_MODULE_INFO OFF)
set(SMTG_RUN_MAC_PKG_BUILD OFF)
set(SMTG_RENAME_ASSERT_TO_WARNING OFF)
set(SMTG_PLUGIN_TARGET_PATH "")
set(SMTG_CREATE_BUNDLE_SYMLINK OFF)
smtg_add_vst3plugin(${target} ${sources})
smtg_target_configure_version_file(${target})

target_link_libraries(${target} PRIVATE
    sdk
    pluginterfaces
    base
    ARA_PlugIn_Library
)

target_include_directories(${target} PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/source/ARA"
    "${CMAKE_CURRENT_SOURCE_DIR}/source/ARA/VST3"
    "${CMAKE_CURRENT_SOURCE_DIR}/ARA_SDK"
    "${CMAKE_CURRENT_SOURCE_DIR}/ARA_SDK/ARA_Examples"
)

target_compile_definitions(${target} PRIVATE
    NOMINMAX
    ARA_MAJOR_VERSION=2
    ARA_MINOR_VERSION=0
    ARA_PATCH_VERSION=0
    ARA_ENABLE_DEBUG_OUTPUT=1
    ARA_VALIDATE_API_CALLS=1
)
