cmake_minimum_required(VERSION 3.22)
project(Dawsoftware VERSION 0.1.0 LANGUAGES CXX)

include(FetchContent)
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.6
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(JUCE)

juce_add_gui_app(Dawsoftware
    PRODUCT_NAME "Dawsoftware"
    COMPANY_NAME "Dawsoftware"
    BUNDLE_ID "com.dawsoftware.app"
)

juce_generate_juce_header(Dawsoftware)

target_sources(Dawsoftware PRIVATE
    Source/Main.cpp
    Source/MainComponent.cpp
)

target_compile_features(Dawsoftware PRIVATE cxx_std_17)
target_compile_definitions(Dawsoftware PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_PLUGINHOST_VST3=1
)

target_link_libraries(Dawsoftware PRIVATE
    juce::juce_audio_utils
    juce::juce_audio_processors
    juce::juce_gui_extra
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags
)