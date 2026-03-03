cmake_minimum_required(VERSION 3.15...3.24)

set(TRACKTION_ROOT "${CMAKE_CURRENT_LIST_DIR}/../ThirdParty/tracktion_engine")
set(LOCAL_JUCE_ROOT "${CMAKE_CURRENT_LIST_DIR}/../ThirdParty/JUCE")
set(LOCAL_JUCE_CMAKE_SUPPORT "${LOCAL_JUCE_ROOT}/extras/Build/CMake/JUCEModuleSupport.cmake")

file(READ "${TRACKTION_ROOT}/VERSION.md" CURRENT_VERSION)
project(BeatMakerApp VERSION ${CURRENT_VERSION})

if(NOT TARGET juce::juce_core)
    if(EXISTS "${LOCAL_JUCE_ROOT}/CMakeLists.txt" AND EXISTS "${LOCAL_JUCE_CMAKE_SUPPORT}")
        add_subdirectory("${LOCAL_JUCE_ROOT}" "${CMAKE_BINARY_DIR}/juce_local_build")
    else()
        message(STATUS "Local JUCE source is incomplete or missing CMake helpers; fetching JUCE via CPM.")
        include("${TRACKTION_ROOT}/cmake/CPM.cmake")
        CPMAddPackage("gh:juce-framework/JUCE#develop")
    endif()
endif()

add_subdirectory("${TRACKTION_ROOT}/modules" "${CMAKE_BINARY_DIR}/tracktion_modules")
add_subdirectory("${CMAKE_CURRENT_LIST_DIR}/Source/BeatMakerNoRecord" "${CMAKE_BINARY_DIR}/BeatMakerNoRecord_build")
