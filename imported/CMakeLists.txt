cmake_minimum_required(VERSION 3.22)
project(EmergentPlugins VERSION 1.0.0 LANGUAGES C CXX)

include(FetchContent)
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.10
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(JUCE)

function(add_emergent_plugin name code bundle_id description)
    juce_add_plugin(${name}
        COMPANY_NAME Emergent
        BUNDLE_ID ${bundle_id}
        PLUGIN_MANUFACTURER_CODE Emrg
        PLUGIN_CODE ${code}
        PRODUCT_NAME ${name}
        DESCRIPTION "${description}"
        FORMATS VST3
    )

    juce_generate_juce_header(${name})

    target_sources(${name} PRIVATE
        ${name}/Source/PluginProcessor.cpp
        ${name}/Source/PluginEditor.cpp
    )
    target_compile_features(${name} PRIVATE cxx_std_17)
    target_compile_options(${name} PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/utf-8>)
    target_compile_definitions(${name} PUBLIC
        JUCE_STRICT_REFCOUNTEDPOINTER=1
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
    )
    target_link_libraries(${name} PRIVATE
        juce::juce_audio_processors
        juce::juce_dsp
        juce::juce_recommended_config_flags
    )
endfunction()

add_emergent_plugin(FETLimiter74 FL74 com.emergent.fetlimiter74 "Vintage FET Limiter Emulation")
add_emergent_plugin(Lead85MKII L85M com.emergent.lead85mkii "Tube Lead Preamp Emulation (Dempwolf model)")
