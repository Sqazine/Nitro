
set(PLUGIN_NAME Oscillator)

juce_add_plugin(${PLUGIN_NAME}
    # VERSION ...                               # Set this if the plugin version is different to the project version
    ICON_BIG  "${JUCE_DIR}/extras/AudioPluginHost/Source/JUCEAppIcon.png"
    # ICON_SMALL ...
    # COMPANY_NAME ...                          # Specify the name of the plugin's author
    IS_SYNTH TRUE                       # Is this a synth or an effect?
    # NEEDS_MIDI_INPUT TRUE/FALSE               # Does the plugin need midi input?
    # NEEDS_MIDI_OUTPUT TRUE/FALSE              # Does the plugin need midi output?
    # IS_MIDI_EFFECT TRUE/FALSE                 # Is this plugin a MIDI effect?
    # EDITOR_WANTS_KEYBOARD_FOCUS TRUE/FALSE    # Does the editor need keyboard focus?
    PLUGIN_MANUFACTURER_CODE Sqaz               # A four-character manufacturer id with at least one upper-case character
    PLUGIN_CODE Sqaz                            # A unique four-character plugin id with exactly one upper-case character
                                                # GarageBand 10.3 requires the first letter to be upper-case, and the remaining letters to be lower-case
    FORMATS VST3
    VST3_AUTO_MANIFEST FALSE
    PRODUCT_NAME "${PLUGIN_NAME}") 

juce_generate_juce_header(${PLUGIN_NAME})


file(GLOB_RECURSE SRC "*.h" "*.cpp" "*.inl")
file(GLOB_RECURSE COMMON_SRC "${CMAKE_SOURCE_DIR}/Common/*.h" "${CMAKE_SOURCE_DIR}/Common/*.cpp" "${CMAKE_SOURCE_DIR}/Common/*.inl")
source_group("${PLUGIN_NAME}" FILES ${SRC})
source_group("Common" FILES ${COMMON_SRC})

target_sources(${PLUGIN_NAME} PRIVATE ${SRC} ${COMMON_SRC})

target_compile_definitions(${PLUGIN_NAME} PUBLIC
                            JUCE_WEB_BROWSER=0  
                            JUCE_USE_CURL=0     
                            JUCE_VST3_CAN_REPLACE_VST2=0)

target_compile_definitions(${PLUGIN_NAME}_VST3 PRIVATE EXPORT_CREATE_FILTER_FUNCTION)
target_sources(${PLUGIN_NAME}_VST3 PRIVATE ${SRC} ${COMMON_SRC})
target_include_directories(${PLUGIN_NAME} PUBLIC ${CMAKE_SOURCE_DIR})

target_link_libraries(${PLUGIN_NAME}
    PRIVATE
        juce::juce_analytics
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

set_property(TARGET ${PLUGIN_NAME}_rc_lib PROPERTY FOLDER ${PLUGIN_NAME})