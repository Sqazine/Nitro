set(PLUGIN_NAME Host)

#==============================================================================
# Sources

# main.cpp holds the entry point (START_JUCE_APPLICATION) of the standalone
# application, everything else is shared between the application and the plugin.
file(GLOB HOST_ALL_SRC "*.h" "*.cpp" "*.inl")

set(HOST_SHARED_SRC)
set(HOST_STANDALONE_SRC)

foreach(host_source IN LISTS HOST_ALL_SRC)
    get_filename_component(host_source_name "${host_source}" NAME)

    if(host_source_name STREQUAL "main.cpp")
        list(APPEND HOST_STANDALONE_SRC "${host_source}")
    else()
        list(APPEND HOST_SHARED_SRC "${host_source}")
    endif()
endforeach()

source_group("${PLUGIN_NAME}" FILES ${HOST_ALL_SRC})

# Definitions that both flavours of the Host need
set(HOST_DEFINITIONS
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

# The internal effects that the Host links in
include(CMakeLinkLibraries.cmake)

#==============================================================================
# VST3 plugin + standalone application out of a single juce_add_plugin call

juce_add_plugin(${PLUGIN_NAME}
    ICON_BIG  "${JUCE_DIR}/extras/AudioPluginHost/Source/JUCEAppIcon.png"
    # IS_SYNTH TRUE/FALSE                       # Is this a synth or an effect?
    NEEDS_MIDI_INPUT TRUE                       # Does the plugin need midi input?
    NEEDS_MIDI_OUTPUT TRUE                      # Does the plugin need midi output?
    # IS_MIDI_EFFECT TRUE/FALSE                 # Is this plugin a MIDI effect?
    # EDITOR_WANTS_KEYBOARD_FOCUS TRUE/FALSE    # Does the editor need keyboard focus?
    MICROPHONE_PERMISSION_ENABLED TRUE          # The standalone app records audio
    PLUGIN_MANUFACTURER_CODE Sqaz               # A four-character manufacturer id with at least one upper-case character
    PLUGIN_CODE Host                            # A unique four-character plugin id with exactly one upper-case character
    FORMATS VST3 Standalone                     # Host.vst3 and Host.exe, from the same sources
    VST3_AUTO_MANIFEST FALSE
    PRODUCT_NAME "${PLUGIN_NAME}")

juce_generate_juce_header(${PLUGIN_NAME})

# The shared code target holds everything but the application entry point,
# every wrapper recompiles the sources just like the other plugins do.
target_sources(${PLUGIN_NAME}             PRIVATE ${HOST_SHARED_SRC})
target_sources(${PLUGIN_NAME}_VST3        PRIVATE ${HOST_SHARED_SRC})
target_sources(${PLUGIN_NAME}_Standalone  PRIVATE ${HOST_SHARED_SRC} ${HOST_STANDALONE_SRC})

target_compile_definitions(${PLUGIN_NAME} PUBLIC
                            JUCE_WEB_BROWSER=0
                            JUCE_USE_CURL=0
                            JUCE_VST3_CAN_REPLACE_VST2=0)

# The shared code target needs JUCE_PLUGINHOST_VST3 so that it compiles the
# bundled VST3 SDK sources (juce_VST3Headers.h only pulls in the SDK
# implementation when JUCE_INTERNAL_HAS_VST3 is set, which is driven by
# JUCE_PLUGINHOST_VST3). The VST3 wrapper then only includes the SDK headers
# and picks the implementation up from here, so both must agree on this flag.
target_compile_definitions(${PLUGIN_NAME} PRIVATE ${HOST_DEFINITIONS})

# EXPORT_CREATE_FILTER_FUNCTION exports createPluginFilter() from the plugin,
# JUCE_USE_CUSTOM_PLUGIN_STANDALONE_APP hands the standalone target over to the
# JUCEApplication defined in main.cpp instead of JUCE's default standalone shell.
target_compile_definitions(${PLUGIN_NAME}_VST3 PRIVATE
                            ${HOST_DEFINITIONS}
                            EXPORT_CREATE_FILTER_FUNCTION)

target_compile_definitions(${PLUGIN_NAME}_Standalone PRIVATE
                            ${HOST_DEFINITIONS}
                            JUCE_USE_CUSTOM_PLUGIN_STANDALONE_APP=1)

target_include_directories(${PLUGIN_NAME} PUBLIC ${CMAKE_SOURCE_DIR})

target_link_libraries(${PLUGIN_NAME}
    PRIVATE
        juce::juce_audio_utils
        juce::juce_cryptography
        juce::juce_dsp
        juce::juce_opengl
        ${AUDIO_EFFECT_LIBS}
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

set_property(TARGET ${PLUGIN_NAME}        PROPERTY FOLDER ${PLUGIN_NAME})
set_property(TARGET ${PLUGIN_NAME}_rc_lib PROPERTY FOLDER ${PLUGIN_NAME})
