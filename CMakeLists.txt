cmake_minimum_required(VERSION 3.22)

project(Nitro VERSION 0.0.1)

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

# Host: exactly like the other units it comes out of a single juce_add_plugin
# call, but it asks for two formats: VST3 (Host.vst3) and Standalone (Host.exe,
# which runs the Host's own JUCEApplication instead of JUCE's default shell).
option(BUILD_Host "Build Host (standalone application and VST3 plugin)" ON)
if(BUILD_Host)
    add_subdirectory(Host)
    
    if(MSVC)
        set_property (DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT Host_Standalone)
    endif(MSVC)
endif(BUILD_Host)

# Helper function for plugin configuration
function(configure_plugin plugin_name)
    option(BUILD_${plugin_name} "Build ${plugin_name} plugin" ON)
    if(BUILD_${plugin_name})
        add_subdirectory(${plugin_name})
    endif(BUILD_${plugin_name})
endfunction()

# Plugins configuration
configure_plugin(AudioPlayer)
configure_plugin(ThreeBandEqualizer)
configure_plugin(Delay)
configure_plugin(Distortion)
configure_plugin(Filter)
configure_plugin(Flanger)
configure_plugin(NoiseGate)
configure_plugin(Oscillator)
configure_plugin(PingPongDelay)
configure_plugin(Reverb)
configure_plugin(Chorus)
configure_plugin(JCM800)
configure_plugin(Tuner)
configure_plugin(TS808)

#DS2
configure_plugin(DS2)

#OD3
configure_plugin(OD3)