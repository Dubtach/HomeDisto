#pragma once
#include <JuceHeader.h>
#include <atomic>

// Simple offline product licensing for Home-Disto.
// The plugin contains only the public RSA verification key. The matching
// private key lives on the seller's website/license-generation service.
class LicenseManager
{
public:
    LicenseManager();

    bool isActivated() const noexcept { return activated.load(std::memory_order_acquire); }
    bool activate(const juce::String& activationCode);
    void deactivate();

    juce::String getLicenseeName() const;
    juce::String getStoredActivationCode() const;

private:
    static constexpr const char* kProductId = "HOMEDISTO";
    static constexpr int kLicenseVersion = 1;

    bool verifyCode(const juce::String& code, juce::String& licenseeOut) const;
    void loadStoredLicense();
    void storeLicense(const juce::String& code, const juce::String& licensee);
    void clearStoredLicense();

    juce::File getLicenseFile() const;

    std::atomic<bool> activated { false };
    juce::String licenseeName;
    juce::String activationCode;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LicenseManager)
};
