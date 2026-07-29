# 1. Mandatory CMake version requirement
cmake_minimum_required(VERSION 3.22)

# Change me!
set(PROJECT_NAME "HomeDisto")

# 2. Mandatory project declaration
project(${PROJECT_NAME} VERSION 1.0.0)

# Worry not, JUCE's PRODUCT_NAME can have spaces (and is what DAWs will display)
set(PRODUCT_NAME "Home Disto")

# Change me! Used for the MacOS bundle name and Installers
set(COMPANY_NAME "Home")

# Change me! Used for the MacOS bundle identifier (and signing)
set(BUNDLE_ID "com.home.homedisto")

# Change me! Set the plugin formats you want built
set(FORMATS Standalone AU VST3 AUv3)

# 3. Mandatory JUCE inclusion (Must precede juce_add_plugin)
# Assumes JUCE is a subdirectory/submodule in your project root
add_subdirectory(JUCE)

juce_add_plugin("${PROJECT_NAME}"
    ICON_BIG "${CMAKE_CURRENT_SOURCE_DIR}/packaging/icon.png"
    COMPANY_NAME "${COMPANY_NAME}"
    BUNDLE_ID "${BUNDLE_ID}"
    COPY_PLUGIN_AFTER_BUILD TRUE
    
    # A four-character plugin id (First character MUST be uppercase for AU)
    PLUGIN_MANUFACTURER_CODE Home
    
    # A unique four-character plugin id
    PLUGIN_CODE Dist
    
    FORMATS ${FORMATS}
    PRODUCT_NAME "${PRODUCT_NAME}"
)

# --- FIX 1: Generate the JuceHeader.h file ---
juce_generate_juce_header("${PROJECT_NAME}")

# 4. Add your C++ source files (Adjust paths if yours differ)
target_sources("${PROJECT_NAME}" PRIVATE
    source/PluginProcessor.cpp
    source/PluginEditor.cpp
)

# 5. Link necessary JUCE modules
target_link_libraries("${PROJECT_NAME}" PRIVATE
    juce::juce_audio_utils
    juce::juce_audio_processors
    juce::juce_dsp # --- FIX 2: Add the DSP module ---
)

# --- FIX: Disables VST2 parameter automation replacement check ---
target_compile_definitions("${PROJECT_NAME}" PUBLIC
    JUCE_VST3_CAN_REPLACE_VST2=0
)