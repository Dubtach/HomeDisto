# Change me!
set(PROJECT_NAME "HomeDisto")

# Worry not, JUCE's PRODUCT_NAME can have spaces (and is what DAWs will display)
set(PRODUCT_NAME "Home Disto")

# Change me! Used for the MacOS bundle name and Installers
set(COMPANY_NAME "Home")

# Change me! Used for the MacOS bundle identifier (and signing)
set(BUNDLE_ID "com.home.homedisto")

# Change me! Set the plugin formats you want built
set(FORMATS Standalone AU VST3 AUv3)

# ... (keep the rest of your CMake setup as is until juce_add_plugin) ...

juce_add_plugin("${PROJECT_NAME}"
    ICON_BIG "${CMAKE_CURRENT_SOURCE_DIR}/packaging/icon.png"
    COMPANY_NAME "${COMPANY_NAME}"
    BUNDLE_ID "${BUNDLE_ID}"
    COPY_PLUGIN_AFTER_BUILD TRUE
    
    # A four-character plugin id (First character MUST be uppercase for AU)
    PLUGIN_MANUFACTURER_CODE Home
    
    # A unique four-character plugin id
    PLUGIN_CODE Dist
    FORMATS "${FORMATS}"
    PRODUCT_NAME "${PRODUCT_NAME}")