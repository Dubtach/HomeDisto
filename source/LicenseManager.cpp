#include "LicenseManager.h"
#include "HomeDistoPublicKey.h"

#include <cstring>

namespace
{
    juce::String sanitizeLicensee(juce::String text)
    {
        text = text.trim().replaceCharacter('|', ' ');

        while (text.contains("  "))
            text = text.replace("  ", " ");

        return text.substring(0, 96).trim();
    }

    bool decodeBase64(const juce::String& text,
                      juce::MemoryBlock& result)
    {
        juce::MemoryOutputStream output;

        if (!juce::Base64::convertFromBase64(output, text))
            return false;

        result = output.getMemoryBlock();

        return !result.isEmpty();
    }

    juce::BigInteger bigIntegerFromBigEndian(
        const juce::MemoryBlock& input)
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


//==============================================================================
LicenseManager::LicenseManager()
{
    loadStoredLicense();
}


//==============================================================================
juce::File LicenseManager::getLicenseFile() const
{
    auto dir =
        juce::File::getSpecialLocation(
            juce::File::userApplicationDataDirectory)
            .getChildFile("Dubtach")
            .getChildFile("Home-Disto");

    dir.createDirectory();

    return dir.getChildFile("license.dat");
}


//==============================================================================
bool LicenseManager::verifyCode(
    const juce::String& code,
    juce::String& licenseeOut) const
{
    // -------------------------------------------------------------
    // Basic activation-code structure
    // -------------------------------------------------------------

    const auto trimmed = code.trim();

    auto parts =
        juce::StringArray::fromTokens(
            trimmed,
            "|",
            {});

    if (parts.size() != 3)
        return false;

    // HD2 only
    if (parts[0] != "HD2")
        return false;


    // -------------------------------------------------------------
    // Decode payload + signature
    // -------------------------------------------------------------

    juce::MemoryBlock payloadBytes;
    juce::MemoryBlock signatureBytes;

    if (!decodeBase64(parts[1], payloadBytes))
        return false;

    if (!decodeBase64(parts[2], signatureBytes))
        return false;


    // -------------------------------------------------------------
    // Decode payload
    // -------------------------------------------------------------

    auto payload =
        juce::String::createStringFromData(
            payloadBytes.getData(),
            static_cast<int>(payloadBytes.getSize()));

    if (payload.isEmpty())
        return false;


    auto payloadParts =
        juce::StringArray::fromTokens(
            payload,
            "|",
            {});

    if (payloadParts.size() != 4)
        return false;


    // HOMEDISTO|2|FULL|Licensee
    if (payloadParts[0] != "HOMEDISTO")
        return false;

    if (payloadParts[1].getIntValue() != 2)
        return false;

    if (payloadParts[2] != "FULL")
        return false;


    // -------------------------------------------------------------
    // Sanitize licensee
    // -------------------------------------------------------------

    auto licensee =
        sanitizeLicensee(payloadParts[3]);

    if (licensee.isEmpty())
        return false;


    // -------------------------------------------------------------
    // Load embedded PUBLIC key
    // -------------------------------------------------------------

    juce::RSAKey publicKey{
    juce::String(HomeDistoLicense::kPublicKey)
};

if (!publicKey.isValid())
    return false;


    // -------------------------------------------------------------
    // Determine RSA key size
    // -------------------------------------------------------------

    auto keyText =
        juce::String(HomeDistoLicense::kPublicKey);

    auto comma =
        keyText.indexOfChar(',');

    if (comma <= 0)
        return false;

    juce::BigInteger modulus;

    modulus.parseString(
        keyText.substring(comma + 1),
        16);

    if (modulus.isZero())
        return false;

    const size_t keyBytes =
        static_cast<size_t>(
            (modulus.getHighestBit() + 1 + 7) / 8);

    // 3072-bit RSA = 384 bytes
    if (signatureBytes.getSize() != keyBytes)
        return false;


    // -------------------------------------------------------------
    // Convert signature to BigInteger
    // -------------------------------------------------------------

    auto signatureValue =
        bigIntegerFromBigEndian(signatureBytes);

    if (signatureValue.isZero())
        return false;


    // -------------------------------------------------------------
    // RSA public-key operation
    // -------------------------------------------------------------

    if (!publicKey.applyToValue(signatureValue))
        return false;


    // -------------------------------------------------------------
    // SHA-256
    // -------------------------------------------------------------

    juce::SHA256 hash(
        payload.getCharPointer());

    juce::MemoryBlock digest;

    digest.loadFromHexString(
        hash.toHexString());

    if (digest.getSize() != 32)
        return false;


    // -------------------------------------------------------------
    // PKCS#1 v1.5 SHA-256 DigestInfo prefix
    //
    // SEQUENCE
    //   SHA-256 AlgorithmIdentifier
    //   OCTET STRING (32-byte digest)
    // -------------------------------------------------------------

    static constexpr unsigned char sha256DigestInfoPrefix[] =
    {
        0x30, 0x31,
        0x30, 0x0D,
        0x06, 0x09,
        0x60, 0x86, 0x48,
        0x01, 0x65,
        0x03, 0x04,
        0x02, 0x01,
        0x05, 0x00,
        0x04, 0x20
    };


    const size_t digestInfoSize =
        sizeof(sha256DigestInfoPrefix) +
        digest.getSize();

    if (keyBytes <
        digestInfoSize + 11)
    {
        return false;
    }


    // -------------------------------------------------------------
    // Construct expected EMSA-PKCS1-v1_5 block
    //
    // 00 01 FF FF FF ... FF 00 DigestInfo
    // -------------------------------------------------------------

    juce::MemoryBlock expectedEncoded(
        keyBytes,
        true);

    auto* encoded =
        static_cast<unsigned char*>(
            expectedEncoded.getData());

    encoded[0] = 0x00;
    encoded[1] = 0x01;


    const size_t paddingSize =
        keyBytes -
        digestInfoSize -
        3;

    std::memset(
        encoded + 2,
        0xFF,
        paddingSize);


    encoded[2 + paddingSize] = 0x00;


    std::memcpy(
        encoded + 3 + paddingSize,
        sha256DigestInfoPrefix,
        sizeof(sha256DigestInfoPrefix));


    digest.copyTo(
        encoded +
            3 +
            paddingSize +
            sizeof(sha256DigestInfoPrefix),
        0,
        digest.getSize());


    // -------------------------------------------------------------
    // Compare decrypted signature against expected block
    // -------------------------------------------------------------

    auto expectedEncodedValue =
        bigIntegerFromBigEndian(
            expectedEncoded);

    if (signatureValue != expectedEncodedValue)
        return false;


    // -------------------------------------------------------------
    // Everything passed
    // -------------------------------------------------------------

    licenseeOut = licensee;

    return true;
}


//==============================================================================
bool LicenseManager::activate(
    const juce::String& activationCodeToStore)
{
    juce::String verifiedLicensee;

    if (!verifyCode(
            activationCodeToStore,
            verifiedLicensee))
    {
        return false;
    }

    activationCode =
        activationCodeToStore.trim();

    licenseeName =
        verifiedLicensee;

    activated.store(
        true,
        std::memory_order_release);

    storeLicense(
        activationCode,
        licenseeName);

    return true;
}


//==============================================================================
void LicenseManager::deactivate()
{
    activated.store(
        false,
        std::memory_order_release);

    activationCode.clear();
    licenseeName.clear();

    clearStoredLicense();
}


//==============================================================================
juce::String LicenseManager::getLicenseeName() const
{
    return licenseeName;
}


//==============================================================================
juce::String LicenseManager::getStoredActivationCode() const
{
    return activationCode;
}


//==============================================================================
void LicenseManager::loadStoredLicense()
{
    auto file =
        getLicenseFile();

    if (!file.existsAsFile())
        return;


    auto xml =
        juce::parseXML(file);

    if (xml == nullptr)
        return;


    auto storedCode =
        xml->getStringAttribute("code").trim();

    if (storedCode.isEmpty())
        return;


    juce::String verifiedLicensee;

    if (verifyCode(
            storedCode,
            verifiedLicensee))
    {
        activationCode =
            storedCode;

        licenseeName =
            verifiedLicensee;

        activated.store(
            true,
            std::memory_order_release);
    }
    else
    {
        clearStoredLicense();
    }
}


//==============================================================================
void LicenseManager::storeLicense(
    const juce::String& code,
    const juce::String& licensee)
{
    auto xml =
        std::make_unique<juce::XmlElement>(
            "HomeDistoLicense");

    xml->setAttribute(
        "code",
        code);

    xml->setAttribute(
        "licensee",
        licensee);

    getLicenseFile()
        .replaceWithText(
            xml->toString());
}


//==============================================================================
void LicenseManager::clearStoredLicense()
{
    getLicenseFile().deleteFile();
}