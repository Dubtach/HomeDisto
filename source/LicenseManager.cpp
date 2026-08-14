#include "LicenseManager.h"

namespace
{
    // Public half of Home-Disto's signing key. NEVER replace this with the
    // private key. The private key belongs only on the license-generation side.
    constexpr const char* kPublicKey =
        "10001,94cb97034443c9a48bde4a216ac4c79a4f395ad9bec6171dd9c0229d225305ce1ec307b4d41f65d25df4d4ef1b5b5eafa820f05b8545d6d9eb4e4fe62800e6f388148c77edac54177764fd7b78953e19901aabc48bd8d6ee3fb28bd3396b44126d5aa66d44d0a3a54d597ca03fb72483c31d9a04c0afbb174f8a62e9796a726da9360113b211f2c84fb46c2b856dfa4805f73699878d8212900079a2a914790dfc3b9a00400ce7a7fc3c9a9c7e56e606b4f7a835a69a3afc20034baf3cd0f7ec973f505bb38b3ebb89747c3644ee8756bed4eb72cf01924f896d02e17e9b36668c2131999f2254a055f3864b3d9a7b13786226e916684c7a7e5d357d19803b11";

    juce::String sanitizeLicensee(juce::String text)
    {
        text = text.trim().replaceCharacter('|', ' ');
        while (text.contains("  "))
            text = text.replace("  ", " ");
        return text.substring(0, 96).trim();
    }

    bool decodeBase64(const juce::String& text, juce::MemoryBlock& result)
    {
        juce::MemoryOutputStream output;
        if (!juce::Base64::convertFromBase64(output, text))
            return false;
        result = output.getMemoryBlock();
        return !result.isEmpty();
    }

    juce::BigInteger bigIntegerFromBigEndian(const juce::MemoryBlock& input)
    {
        juce::MemoryBlock littleEndian;
        littleEndian.setSize(input.getSize(), false);
        for (size_t i = 0; i < input.getSize(); ++i)
            littleEndian[i] = input[input.getSize() - 1 - i];

        juce::BigInteger result;
        result.loadFromMemoryBlock(littleEndian);
        return result;
    }
}

LicenseManager::LicenseManager()
{
    loadStoredLicense();
}

juce::File LicenseManager::getLicenseFile() const
{
    auto dir = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                    .getChildFile("Dubtach")
                    .getChildFile("Home-Disto");
    dir.createDirectory();
    return dir.getChildFile("license.dat");
}

bool LicenseManager::verifyCode(const juce::String& code, juce::String& licenseeOut) const
{
    auto trimmed = code.trim();
    auto parts = juce::StringArray::fromTokens(trimmed, "|", {});
    if (parts.size() != 3 || parts[0] != "HD1")
        return false;

    juce::MemoryBlock payloadBytes, signatureBytes;
    if (!decodeBase64(parts[1], payloadBytes) || !decodeBase64(parts[2], signatureBytes))
        return false;

    if (signatureBytes.getSize() != 256)
        return false;

    auto payload = juce::String::createStringFromData(payloadBytes.getData(), (int) payloadBytes.getSize());
    if (payload.isEmpty())
        return false;

    auto payloadParts = juce::StringArray::fromTokens(payload, "|", {});
    if (payloadParts.size() != 4)
        return false;
    if (payloadParts[0] != kProductId || payloadParts[1].getIntValue() != kLicenseVersion || payloadParts[2] != "FULL")
        return false;

    auto licensee = sanitizeLicensee(payloadParts[3]);
    if (licensee.isEmpty())
        return false;

    juce::RSAKey publicKey { juce::String(kPublicKey) };
    if (!publicKey.isValid())
        return false;

    // Signatures are stored as big-endian RSA integers. JUCE BigInteger uses
    // little-endian memory blocks, so convert before verification.
    auto signatureValue = bigIntegerFromBigEndian(signatureBytes);
    if (signatureValue.isZero())
        return false;

    if (!publicKey.applyToValue(signatureValue))
        return false;

    juce::SHA256 hash { payload.getCharPointer() };
    juce::BigInteger expectedHash;
    expectedHash.parseString(hash.toHexString(), 16);

    if (signatureValue != expectedHash)
        return false;

    licenseeOut = licensee;
    return true;
}

bool LicenseManager::activate(const juce::String& activationCodeToStore)
{
    juce::String verifiedLicensee;
    if (!verifyCode(activationCodeToStore, verifiedLicensee))
        return false;

    activationCode = activationCodeToStore.trim();
    licenseeName = verifiedLicensee;
    activated.store(true, std::memory_order_release);
    storeLicense(activationCode, licenseeName);
    return true;
}

void LicenseManager::deactivate()
{
    activated.store(false, std::memory_order_release);
    activationCode.clear();
    licenseeName.clear();
    clearStoredLicense();
}

juce::String LicenseManager::getLicenseeName() const
{
    return licenseeName;
}

juce::String LicenseManager::getStoredActivationCode() const
{
    return activationCode;
}

void LicenseManager::loadStoredLicense()
{
    auto file = getLicenseFile();
    if (!file.existsAsFile())
        return;

    auto xml = juce::parseXML(file);
    if (xml == nullptr)
        return;

    auto storedCode = xml->getStringAttribute("code").trim();
    if (storedCode.isEmpty())
        return;

    juce::String verifiedLicensee;
    if (verifyCode(storedCode, verifiedLicensee))
    {
        activationCode = storedCode;
        licenseeName = verifiedLicensee;
        activated.store(true, std::memory_order_release);
    }
    else
    {
        clearStoredLicense();
    }
}

void LicenseManager::storeLicense(const juce::String& code, const juce::String& licensee)
{
    auto xml = std::make_unique<juce::XmlElement>("HomeDistoLicense");
    xml->setAttribute("code", code);
    xml->setAttribute("licensee", licensee);
    getLicenseFile().replaceWithText(xml->toString());
}

void LicenseManager::clearStoredLicense()
{
    getLicenseFile().deleteFile();
}