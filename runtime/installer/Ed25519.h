#pragma once

#include <cctype>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include <openssl/err.h>
#include <openssl/evp.h>

namespace neuron::installer::crypto {

inline std::string openSslErrorText() {
  const unsigned long err = ERR_get_error();
  if (err == 0) {
    return "unknown OpenSSL error";
  }
  char buffer[256];
  ERR_error_string_n(err, buffer, sizeof(buffer));
  return std::string(buffer);
}

inline bool readFileBytes(const std::filesystem::path &path,
                          std::vector<uint8_t> *outBytes,
                          std::string *outError) {
  if (outBytes == nullptr) {
    if (outError != nullptr) {
      *outError = "internal error: null output buffer";
    }
    return false;
  }

  std::ifstream file(path, std::ios::binary | std::ios::ate);
  if (!file.is_open()) {
    if (outError != nullptr) {
      *outError = "failed to open file: " + path.string();
    }
    return false;
  }

  const std::streamoff size = file.tellg();
  if (size < 0) {
    if (outError != nullptr) {
      *outError = "failed to read file size: " + path.string();
    }
    return false;
  }
  file.seekg(0, std::ios::beg);

  outBytes->resize(static_cast<size_t>(size));
  if (!outBytes->empty()) {
    file.read(reinterpret_cast<char *>(outBytes->data()),
              static_cast<std::streamsize>(outBytes->size()));
    if (!file.good()) {
      if (outError != nullptr) {
        *outError = "failed to read file bytes: " + path.string();
      }
      return false;
    }
  }
  return true;
}

inline bool decodeHex(const std::string &hex, std::vector<uint8_t> *outBytes,
                      std::string *outError) {
  if (outBytes == nullptr) {
    if (outError != nullptr) {
      *outError = "internal error: null output buffer";
    }
    return false;
  }

  std::string cleaned;
  cleaned.reserve(hex.size());
  for (char ch : hex) {
    if (std::isspace(static_cast<unsigned char>(ch))) {
      continue;
    }
    cleaned.push_back(ch);
  }

  if ((cleaned.size() % 2u) != 0u) {
    if (outError != nullptr) {
      *outError = "hex input must have even length";
    }
    return false;
  }

  auto fromHex = [](char c) -> int {
    if (c >= '0' && c <= '9') {
      return c - '0';
    }
    if (c >= 'a' && c <= 'f') {
      return c - 'a' + 10;
    }
    if (c >= 'A' && c <= 'F') {
      return c - 'A' + 10;
    }
    return -1;
  };

  outBytes->clear();
  outBytes->reserve(cleaned.size() / 2u);
  for (size_t i = 0; i < cleaned.size(); i += 2u) {
    const int hi = fromHex(cleaned[i]);
    const int lo = fromHex(cleaned[i + 1u]);
    if (hi < 0 || lo < 0) {
      if (outError != nullptr) {
        *outError = "invalid hex character in input";
      }
      return false;
    }
    outBytes->push_back(static_cast<uint8_t>((hi << 4) | lo));
  }

  return true;
}

inline bool decodeBase64(const std::string &input,
                         std::vector<uint8_t> *outBytes,
                         std::string *outError) {
  if (outBytes == nullptr) {
    if (outError != nullptr) {
      *outError = "internal error: null output buffer";
    }
    return false;
  }

  std::string cleaned;
  cleaned.reserve(input.size());
  for (char ch : input) {
    if (!std::isspace(static_cast<unsigned char>(ch))) {
      cleaned.push_back(ch);
    }
  }

  if (cleaned.empty()) {
    outBytes->clear();
    return true;
  }

  std::vector<unsigned char> decoded(
      ((cleaned.size() + 3u) / 4u) * 3u + 4u, 0u);
  const int rawSize =
      EVP_DecodeBlock(decoded.data(),
                      reinterpret_cast<const unsigned char *>(cleaned.data()),
                      static_cast<int>(cleaned.size()));
  if (rawSize < 0) {
    if (outError != nullptr) {
      *outError = "invalid base64 input";
    }
    return false;
  }

  size_t outputSize = static_cast<size_t>(rawSize);
  if (!cleaned.empty() && cleaned.back() == '=') {
    outputSize--;
    if (cleaned.size() >= 2u && cleaned[cleaned.size() - 2u] == '=') {
      outputSize--;
    }
  }
  outBytes->assign(decoded.begin(), decoded.begin() + outputSize);
  return true;
}

inline bool decodeKeyOrSignature(const std::string &value,
                                 std::vector<uint8_t> *outBytes,
                                 std::string *outError) {
  bool maybeHex = !value.empty();
  for (char ch : value) {
    if (std::isspace(static_cast<unsigned char>(ch))) {
      continue;
    }
    if (!std::isxdigit(static_cast<unsigned char>(ch))) {
      maybeHex = false;
      break;
    }
  }

  if (maybeHex) {
    return decodeHex(value, outBytes, outError);
  }
  return decodeBase64(value, outBytes, outError);
}

inline bool verifyEd25519(const uint8_t *message, size_t messageSize,
                          const uint8_t *signature, size_t signatureSize,
                          const uint8_t *publicKey, size_t publicKeySize,
                          std::string *outError) {
  if (signatureSize != 64u) {
    if (outError != nullptr) {
      *outError = "invalid Ed25519 signature size (expected 64 bytes)";
    }
    return false;
  }
  if (publicKeySize != 32u) {
    if (outError != nullptr) {
      *outError = "invalid Ed25519 public key size (expected 32 bytes)";
    }
    return false;
  }

  EVP_PKEY *pkey = EVP_PKEY_new_raw_public_key(EVP_PKEY_ED25519, nullptr,
                                               publicKey, publicKeySize);
  if (pkey == nullptr) {
    if (outError != nullptr) {
      *outError = "failed to create Ed25519 public key: " + openSslErrorText();
    }
    return false;
  }

  EVP_MD_CTX *ctx = EVP_MD_CTX_new();
  if (ctx == nullptr) {
    EVP_PKEY_free(pkey);
    if (outError != nullptr) {
      *outError = "failed to allocate OpenSSL digest context";
    }
    return false;
  }

  bool ok = false;
  if (EVP_DigestVerifyInit(ctx, nullptr, nullptr, nullptr, pkey) != 1) {
    if (outError != nullptr) {
      *outError = "EVP_DigestVerifyInit failed: " + openSslErrorText();
    }
  } else {
    const int verifyResult =
        EVP_DigestVerify(ctx, signature, signatureSize, message, messageSize);
    if (verifyResult == 1) {
      ok = true;
    } else if (outError != nullptr) {
      *outError = "signature verification failed";
    }
  }

  EVP_MD_CTX_free(ctx);
  EVP_PKEY_free(pkey);
  return ok;
}

inline bool verifyEd25519Encoded(const std::vector<uint8_t> &message,
                                 const std::string &signatureEncoded,
                                 const std::string &publicKeyEncoded,
                                 std::string *outError) {
  std::vector<uint8_t> signature;
  if (!decodeKeyOrSignature(signatureEncoded, &signature, outError)) {
    return false;
  }

  std::vector<uint8_t> publicKey;
  if (!decodeKeyOrSignature(publicKeyEncoded, &publicKey, outError)) {
    return false;
  }

  return verifyEd25519(message.data(), message.size(), signature.data(),
                       signature.size(), publicKey.data(), publicKey.size(),
                       outError);
}

inline bool verifyEd25519File(const std::filesystem::path &path,
                              const std::string &signatureEncoded,
                              const std::string &publicKeyEncoded,
                              std::string *outError) {
  std::vector<uint8_t> fileBytes;
  if (!readFileBytes(path, &fileBytes, outError)) {
    return false;
  }
  return verifyEd25519Encoded(fileBytes, signatureEncoded, publicKeyEncoded,
                              outError);
}

} // namespace neuron::installer::crypto

