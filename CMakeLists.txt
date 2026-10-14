cmake_minimum_required(VERSION 3.22)

# Cible macOS minimale (doit être défini avant project())
set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Version minimale de macOS")

project(Reverie VERSION 1.0.0)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
# JUCE est téléchargé automatiquement ; mettez JUCE_DIR pour utiliser une copie locale
if(DEFINED JUCE_DIR AND EXISTS "${JUCE_DIR}/CMakeLists.txt")
    add_subdirectory("${JUCE_DIR}" JUCE)
else()
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG        8.0.9
        GIT_SHALLOW    TRUE)
    FetchContent_MakeAvailable(JUCE)
endif()

# Copie automatique du plugin dans le dossier système (sauf sur un serveur d'intégration continue)
if(DEFINED ENV{CI})
    set(REVERIE_COPY FALSE)
else()
    set(REVERIE_COPY TRUE)
endif()

set(REVERIE_FORMATS VST3 Standalone)
if(APPLE)
    list(APPEND REVERIE_FORMATS AU)   # AU : pour Logic Pro / GarageBand
endif()

juce_add_plugin(Reverie
    COMPANY_NAME                "Reverie Audio"
    PLUGIN_MANUFACTURER_CODE    Rvrx
    PLUGIN_CODE                 Rvr1
    FORMATS                     ${REVERIE_FORMATS}
    PRODUCT_NAME                "Reverie"
    IS_SYNTH                    FALSE
    NEEDS_MIDI_INPUT            FALSE
    NEEDS_MIDI_OUTPUT           FALSE
    VST3_CATEGORIES             Fx Reverb Delay
    AU_MAIN_TYPE                kAudioUnitType_Effect
    COPY_PLUGIN_AFTER_BUILD     ${REVERIE_COPY})

target_sources(Reverie PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginProcessor.h
    Source/PluginEditor.cpp
    Source/PluginEditor.h
    Source/Parameters.h
    Source/DSP.h)

target_compile_definitions(Reverie PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_STRICT_REFCOUNTEDPOINTER=1)

if(MSVC)
    target_compile_options(Reverie PRIVATE /utf-8)
endif()

target_link_libraries(Reverie
    PRIVATE
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)
