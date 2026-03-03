cmake_minimum_required(VERSION 3.15)

set (TARGET_NAME TheSampledexWorkflow)
option (SAMPLEXDEX_WINDOWS_BUILD "Enable Windows-oriented build toggles" OFF)

if (MSVC)
    set (CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

if (${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
    message ("-- building for macOS")
    set (CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE INTERNAL "")
elseif(${CMAKE_SYSTEM_NAME} MATCHES "iOS")
    set (CMAKE_XCODE_ATTRIBUTE_IPHONEOS_DEPLOYMENT_TARGET "17.0" CACHE INTERNAL "")
    set (CMAKE_XCODE_ATTRIBUTE_CODE_SIGN_IDENTITY "Apple Development" CACHE INTERNAL "")
endif()

set_property(GLOBAL PROPERTY USE_FOLDERS YES)
option(JUCE_ENABLE_MODULE_SOURCE_GROUPS "Enable Module Source Groups" ON)

set (SAMPLEXDEX_ENABLE_AU_HOST 0)
if (APPLE)
    set (SAMPLEXDEX_ENABLE_AU_HOST 1)
endif()

juce_add_gui_app(${TARGET_NAME}
    BUNDLE_ID "com.sampledex.TheSampledexWorkflow"
    MICROPHONE_PERMISSION_ENABLED FALSE
    REQUIRES_FULL_SCREEN TRUE
    STATUS_BAR_HIDDEN TRUE
    PLUGINHOST_AU ${SAMPLEXDEX_ENABLE_AU_HOST})

juce_generate_juce_header(${TARGET_NAME})

target_compile_features(${TARGET_NAME} PRIVATE cxx_std_20)

set_target_properties(${TARGET_NAME} PROPERTIES
    C_VISIBILITY_PRESET hidden
    CXX_VISIBILITY_PRESET hidden)

set (SourceFiles
    BeatMakerNoRecord.h
    BeatMakerNoRecord.cpp
    DawScaffold.h
    DawScaffold.cpp)

target_sources(${TARGET_NAME} PRIVATE ${SourceFiles})
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR}/.. PREFIX Source FILES ${SourceFiles})

target_compile_definitions(${TARGET_NAME} PRIVATE
    JUCE_PLUGINHOST_AU=${SAMPLEXDEX_ENABLE_AU_HOST}
    JUCE_PLUGINHOST_VST3=1
    JUCE_USE_CURL=0
    JUCE_WEB_BROWSER=0
    JUCE_MODAL_LOOPS_PERMITTED=1
    JUCE_STRICT_REFCOUNTEDPOINTER=1
    TRACKTION_ENABLE_TIMESTRETCH_SOUNDTOUCH=1)

if (WIN32 OR SAMPLEXDEX_WINDOWS_BUILD)
    target_compile_definitions (${TARGET_NAME} PRIVATE
        JUCE_DIRECTSOUND=1
        JUCE_WASAPI=1
        JUCE_ASIO=1)
endif()

target_link_libraries(${TARGET_NAME} PRIVATE
    tracktion::tracktion_core
    tracktion::tracktion_engine
    tracktion::tracktion_graph
    juce::juce_audio_devices
    juce::juce_audio_processors
    juce::juce_audio_utils
    juce::juce_recommended_warning_flags)

if (CMAKE_CXX_COMPILER_ID MATCHES "Clang" OR
    CMAKE_CXX_COMPILER_ID MATCHES "GNU")
    target_compile_options(${TARGET_NAME} PRIVATE "$<$<CONFIG:Release>:-Werror>")
elseif (CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
    target_compile_options(${TARGET_NAME} PRIVATE "/W4")
    target_compile_options(${TARGET_NAME} PRIVATE "$<$<CONFIG:Release>:/WX>")
endif()

if (${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
    target_compile_options(${TARGET_NAME} PRIVATE "-fno-aligned-allocation")
endif()

if (CMAKE_CXX_COMPILER_ID MATCHES "GNU")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-unknown-pragmas")
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_link_libraries(${TARGET_NAME} PRIVATE "-latomic")

    if (NOT ${CMAKE_HOST_SYSTEM_PROCESSOR} MATCHES "aarch64")
        target_link_options(${TARGET_NAME} PRIVATE "-m64")
    endif()
endif()

if (BUILD_TESTING)
    set (UI_REGRESSION_TEST_TARGET BeatMakerNoRecordUiRegressionTests)
    add_executable (${UI_REGRESSION_TEST_TARGET}
        Panels/SharedLayoutSystem.cpp
        UI/BeatMakerNoRecordCommandRouting.cpp
        Tests/UiRegressionTests.cpp)

    target_compile_features (${UI_REGRESSION_TEST_TARGET} PRIVATE cxx_std_20)
    target_include_directories (${UI_REGRESSION_TEST_TARGET} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries (${UI_REGRESSION_TEST_TARGET} PRIVATE
        juce::juce_gui_basics
        juce::juce_core)

    add_test (NAME ${UI_REGRESSION_TEST_TARGET}
              COMMAND ${UI_REGRESSION_TEST_TARGET})
endif()
