set(EXE_NAME Host)

juce_add_gui_app(${EXE_NAME} 
                MICROPHONE_PERMISSION_ENABLED TRUE
                ICON_BIG  "${JUCE_DIR}/extras/AudioPluginHost/Source/JUCEAppIcon.png")

juce_generate_juce_header(${EXE_NAME})

file(GLOB SRC "*.h" "*.cpp" "*.inl")
source_group("${EXE_NAME}" FILES ${SRC})

target_sources(${EXE_NAME} PRIVATE ${SRC})

target_compile_definitions(${EXE_NAME} PRIVATE
    JUCE_ALSA=1
    JUCE_CONTENT_SHARING=1
    JUCE_DIRECTSOUND=1
    JUCE_DISABLE_CAUTIOUS_PARAMETER_ID_CHECKING=1
    JUCE_PLUGINHOST_LADSPA=1
    JUCE_PLUGINHOST_LV2=1
    JUCE_PLUGINHOST_VST3=1
    JUCE_PLUGINHOST_VST=0
    JUCE_PLUGINHOST_ARA=0
    JUCE_USE_CAMERA=0
    JUCE_USE_CDBURNER=0
    JUCE_USE_CDREADER=0
    JUCE_USE_CURL=0
    JUCE_USE_FLAC=0
    JUCE_USE_OGGVORBIS=1
    JUCE_VST3_HOST_CROSS_PLATFORM_UID=1
    JUCE_WASAPI=1
    JUCE_WEB_BROWSER=0
    # This is a temporary workaround to allow builds to complete on Xcode 15.
    # Add -Wl,-ld_classic to the OTHER_LDFLAGS build setting if you need to
    # deploy to older versions of macOS/iOS.
    JUCE_SILENCE_XCODE_15_LINKER_WARNING=1)

target_link_libraries(${EXE_NAME} PRIVATE
    juce::juce_audio_utils
    juce::juce_cryptography
    juce::juce_dsp
    juce::juce_opengl
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags)

set_property(TARGET ${EXE_NAME}_rc_lib PROPERTY FOLDER ${EXE_NAME})
set_property(TARGET ${EXE_NAME} PROPERTY FOLDER ${EXE_NAME})

target_include_directories(${EXE_NAME} PUBLIC ${CMAKE_SOURCE_DIR})

include(CMakeLinkLibraries.cmake)

target_link_libraries(${EXE_NAME} PRIVATE ${AUDIO_EFFECT_LIBS})