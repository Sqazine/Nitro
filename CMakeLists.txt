cmake_minimum_required(VERSION 3.22)

project(LabAudioEffect VERSION 0.0.1)

set_directory_properties(PROPERTIES
    JUCE_COMPANY_NAME       "Sqazine"
    JUCE_COMPANY_EMAIL      "Sqazine@163.com")


set(JUCE_DIR ${CMAKE_SOURCE_DIR}/JUCE)

add_subdirectory(${JUCE_DIR} ${CMAKE_BINARY_DIR}/JUCE EXCLUDE_FROM_ALL)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}/Bin/Debug)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_BINARY_DIR}/Bin/Release)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO ${CMAKE_BINARY_DIR}/Bin/RelWithDebInfo)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}/Bin/Debug)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_RELEASE ${CMAKE_BINARY_DIR}/Bin/Release)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_RELWITHDEBINFO ${CMAKE_BINARY_DIR}/Bin/RelWithDebInfo)

add_subdirectory(Host)

if(MSVC)
    set_property (DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT Host)
endif()

add_subdirectory(AudioPlayer)
add_subdirectory(ThreeBandEqualizer)
add_subdirectory(Delay)
add_subdirectory(Distortion)
add_subdirectory(Filter)
add_subdirectory(Flanger)
add_subdirectory(NoiseGate)
add_subdirectory(Oscillator)
add_subdirectory(PingPongDelay)
add_subdirectory(Reverb)
add_subdirectory(SimpleDistortion)
add_subdirectory(SimpleEQ)
add_subdirectory(Chorus)