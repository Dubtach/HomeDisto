# 1. Mandatory CMake version requirement
cmake_minimum_required(VERSION 3.22)

# Change me![cite: 13]
set(PROJECT_NAME "HomeDisto") #[cite: 13]

# 2. Mandatory project declaration
project(${PROJECT_NAME} VERSION 1.0.0)

# Worry not, JUCE's PRODUCT_NAME can have spaces (and is what DAWs will display)[cite: 13]
set(PRODUCT_NAME "Home Disto") #[cite: 13]

# Change me! Used for the MacOS bundle name and Installers[cite: 13]
set(COMPANY_NAME "Home") #[cite: 13]

# Change me! Used for the MacOS bundle identifier (and signing)[cite: 13]
set(BUNDLE_ID "com.home.homedisto") #[cite: 13]

# Change me! Set the plugin formats you want built[cite: 13]
set(FORMATS Standalone AU VST3 AUv3) #[cite: 13]

# 3. Mandatory JUCE inclusion (Must precede juce_add_plugin)
# Assumes JUCE is a subdirectory/submodule in your project root
add_subdirectory(JUCE)

juce_add_plugin("${PROJECT_NAME}"
    ICON_BIG "${CMAKE_CURRENT_SOURCE_DIR}/packaging/icon.png" #[cite: 13]
    COMPANY_NAME "${COMPANY_NAME}" #[cite: 13]
    BUNDLE_ID "${BUNDLE_ID}" #[cite: 13]
    COPY_PLUGIN_AFTER_BUILD TRUE #[cite: 13]
    
    # A four-character plugin id (First character MUST be uppercase for AU)[cite: 13]
    PLUGIN_MANUFACTURER_CODE Home #[cite: 13]
    
    # A unique four-character plugin id[cite: 13]
    PLUGIN_CODE Dist #[cite: 13]
    
    FORMATS ${FORMATS} #[cite: 13]
    PRODUCT_NAME "${PRODUCT_NAME}" #[cite: 13]
)

# 4. Add your C++ source files (Adjust paths if yours differ)
target_sources("${PROJECT_NAME}" PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
)

# 5. Link necessary JUCE modules
target_link_libraries("${PROJECT_NAME}" PRIVATE
    juce::juce_audio_utils
    juce::juce_audio_processors
)