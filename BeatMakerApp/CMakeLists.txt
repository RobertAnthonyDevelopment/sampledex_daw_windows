cmake_minimum_required(VERSION 3.15...3.24)

set(TRACKTION_ROOT "${CMAKE_CURRENT_LIST_DIR}/../ThirdParty/tracktion_engine")
set(LOCAL_JUCE_ROOT "${CMAKE_CURRENT_LIST_DIR}/../ThirdParty/JUCE")

file(READ "${TRACKTION_ROOT}/VERSION.md" CURRENT_VERSION)
project(BeatMakerApp VERSION ${CURRENT_VERSION})

if(NOT TARGET juce::juce_core)
    if(EXISTS "${LOCAL_JUCE_ROOT}/CMakeLists.txt")
        add_subdirectory("${LOCAL_JUCE_ROOT}" "${CMAKE_BINARY_DIR}/juce_local_build")
    else()
        include("${TRACKTION_ROOT}/cmake/CPM.cmake")
        CPMAddPackage("gh:juce-framework/JUCE#develop")
    endif()
endif()

add_subdirectory("${TRACKTION_ROOT}/modules" "${CMAKE_BINARY_DIR}/tracktion_modules")
add_subdirectory("${CMAKE_CURRENT_LIST_DIR}/Source/BeatMakerNoRecord" "${CMAKE_BINARY_DIR}/BeatMakerNoRecord_build")
