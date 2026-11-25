cmake_minimum_required(VERSION 3.22)
project(EmergentGuiPreview VERSION 1.0.0 LANGUAGES C CXX)

# Use the same JUCE dependency and source paths as the VST3 build.
add_subdirectory(.. imported-plugins)
juce_add_console_app(EmergentGuiPreview)
juce_generate_juce_header(EmergentGuiPreview)
target_sources(EmergentGuiPreview PRIVATE Capture.cpp)
target_compile_features(EmergentGuiPreview PRIVATE cxx_std_17)
target_compile_options(EmergentGuiPreview PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/utf-8>)
target_compile_definitions(EmergentGuiPreview PRIVATE
    JUCE_STRICT_REFCOUNTEDPOINTER=1 JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
target_link_libraries(EmergentGuiPreview PRIVATE
    juce::juce_audio_processors juce::juce_dsp juce::juce_recommended_config_flags Lead85IRS)

foreach(plugin IN ITEMS FETLimiter74 Lead85MKII)
    set(source_dir "${CMAKE_CURRENT_LIST_DIR}/../${plugin}/Source")
    set(processor "${source_dir}/PluginProcessor.cpp")
    set(editor "${source_dir}/PluginEditor.cpp")
    target_sources(EmergentGuiPreview PRIVATE "${processor}" "${editor}")
    set_source_files_properties("${processor}" "${editor}" PROPERTIES
        COMPILE_DEFINITIONS "JucePlugin_Name=\"${plugin}\";createPluginFilter=create${plugin}")
endforeach()
