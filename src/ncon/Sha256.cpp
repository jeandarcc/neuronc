#include "neuronc/ncon/Sha256.h"

#include <algorithm>
#include <array>
#include <fstream>
#include <iomanip>
#include <sstream>

namespace neuron::ncon {

namespace {

constexpr std::array<uint32_t, 64> kConstants = {
    0x428a2f98u, 0x71374491u, 0xb5c0fbcfu, 0xe9b5dba5u, 0x3956c25bu,
    0x59f111f1u, 0x923f82a4u, 0xab1c5ed5u, 0xd807aa98u, 0x12835b01u,
    0x243185beu, 0x550c7dc3u, 0x72be5d74u, 0x80deb1feu, 0x9bdc06a7u,
    0xc19bf174u, 0xe49b69c1u, 0xefbe4786u, 0x0fc19dc6u, 0x240ca1ccu,
    0x2de92c6fu, 0x4a7484aau, 0x5cb0a9dcu, 0x76f988dau, 0x983e5152u,
    0xa831c66du, 0xb00327c8u, 0xbf597fc7u, 0xc6e00bf3u, 0xd5a79147u,
    0x06ca6351u, 0x14292967u, 0x27b70a85u, 0x2e1b2138u, 0x4d2c6dfcu,
    0x53380d13u, 0x650a7354u, 0x766a0abbu, 0x81c2c92eu, 0x92722c85u,
    0xa2bfe8a1u, 0xa81a664bu, 0xc24b8b70u, 0xc76c51a3u, 0xd192e819u,
    0xd6990624u, 0xf40e3585u, 0x106aa070u, 0x19a4c116u, 0x1e376c08u,
    0x2748774cu, 0x34b0bcb5u, 0x391c0cb3u, 0x4ed8aa4au, 0x5b9cca4fu,
    0x682e6ff3u, 0x748f82eeu, 0x78a5636fu, 0x84c87814u, 0x8cc70208u,
    0x90befffau, 0xa4506cebu, 0xbef9a3f7u, 0xc67178f2u,
};

inline uint32_t rotr(uint32_t value, uint32_t bits) {
  return (value >> bits) | (value << (32u - bits));
}

class Sha256 {
public:
  Sha256() { reset(); }

  void reset() {
    m_state = {0x6a09e667u, 0xbb67ae85u, 0x3c6ef372u, 0xa54ff53au,
               0x510e527fu, 0x9b05688cu, 0x1f83d9abu, 0x5be0cd19u};
    m_bufferLen = 0;
    m_totalBytes = 0;
  }

  void update(const uint8_t *data, size_t size) {
    if (data == nullptr || size == 0) {
      return;
    }
    m_totalBytes += static_cast<uint64_t>(size);

    size_t offset = 0;
    while (offset < size) {
      const size_t toCopy = std::min<size_t>(64u - m_bufferLen, size - offset);
      for (size_t i = 0; i < toCopy; ++i) {
        m_buffer[m_bufferLen + i] = data[offset + i];
      }
      m_bufferLen += toCopy;
      offset += toCopy;
      if (m_bufferLen == 64u) {
        processBlock(m_buffer.data());
        m_bufferLen = 0;
      }
    }
  }

  std::array<uint8_t, 32> final() {
    std::array<uint8_t, 32> digest{};
    const uint64_t totalBits = m_totalBytes * 8u;

    m_buffer[m_bufferLen++] = 0x80u;
    if (m_bufferLen > 56u) {
      while (m_bufferLen < 64u) {
        m_buffer[m_bufferLen++] = 0u;
      }
      processBlock(m_buffer.data());
      m_bufferLen = 0;
    }

    while (m_bufferLen < 56u) {
      m_buffer[m_bufferLen++] = 0u;
    }

    for (int i = 7; i >= 0; --i) {
      m_buffer[m_bufferLen++] =
          static_cast<uint8_t>((totalBits >> (static_cast<uint64_t>(i) * 8u)) &
                               0xFFu);
    }
    processBlock(m_buffer.data());
    m_bufferLen = 0;

    for (size_t i = 0; i < m_state.size(); ++i) {
      digest[i * 4 + 0] = static_cast<uint8_t>((m_state[i] >> 24u) & 0xFFu);
      digest[i * 4 + 1] = static_cast<uint8_t>((m_state[i] >> 16u) & 0xFFu);
      digest[i * 4 + 2] = static_cast<uint8_t>((m_state[i] >> 8u) & 0xFFu);
      digest[i * 4 + 3] = static_cast<uint8_t>(m_state[i] & 0xFFu);
    }
    return digest;
  }

private:
  void processBlock(const uint8_t *block) {
    std::array<uint32_t, 64> words{};
    for (size_t i = 0; i < 16; ++i) {
      const size_t base = i * 4;
      words[i] = (static_cast<uint32_t>(block[base]) << 24u) |
                 (static_cast<uint32_t>(block[base + 1]) << 16u) |
                 (static_cast<uint32_t>(block[base + 2]) << 8u) |
                 static_cast<uint32_t>(block[base + 3]);
    }
    for (size_t i = 16; i < 64; ++i) {
      const uint32_t s0 =
          rotr(words[i - 15], 7u) ^ rotr(words[i - 15], 18u) ^
          (words[i - 15] >> 3u);
      const uint32_t s1 =
          rotr(words[i - 2], 17u) ^ rotr(words[i - 2], 19u) ^
          (words[i - 2] >> 10u);
      words[i] = words[i - 16] + s0 + words[i - 7] + s1;
    }

    uint32_t a = m_state[0];
    uint32_t b = m_state[1];
    uint32_t c = m_state[2];
    uint32_t d = m_state[3];
    uint32_t e = m_state[4];
    uint32_t f = m_state[5];
    uint32_t g = m_state[6];
    uint32_t h = m_state[7];

    for (size_t i = 0; i < 64; ++i) {
      const uint32_t s1 = rotr(e, 6u) ^ rotr(e, 11u) ^ rotr(e, 25u);
      const uint32_t choose = (e & f) ^ ((~e) & g);
      const uint32_t temp1 = h + s1 + choose + kConstants[i] + words[i];
      const uint32_t s0 = rotr(a, 2u) ^ rotr(a, 13u) ^ rotr(a, 22u);
      const uint32_t majority = (a & b) ^ (a & c) ^ (b & c);
      const uint32_t temp2 = s0 + majority;

      h = g;
      g = f;
      f = e;
      e = d + temp1;
      d = c;
      c = b;
      b = a;
      a = temp1 + temp2;
    }

    m_state[0] += a;
    m_state[1] += b;
    m_state[2] += c;
    m_state[3] += d;
    m_state[4] += e;
    m_state[5] += f;
    m_state[6] += g;
    m_state[7] += h;
  }

  std::array<uint32_t, 8> m_state{};
  std::array<uint8_t, 64> m_buffer{};
  size_t m_bufferLen = 0;
  uint64_t m_totalBytes = 0;
};

std::string toHex(const std::array<uint8_t, 32> &digest) {
  std::ostringstream out;
  out << std::hex << std::setfill('0');
  for (uint8_t byte : digest) {
    out << std::setw(2) << static_cast<unsigned>(byte);
  }
  return out.str();
}

} // namespace

std::string sha256Hex(const std::vector<uint8_t> &bytes) {
  Sha256 hash;
  if (!bytes.empty()) {
    hash.update(bytes.data(), bytes.size());
  }
  return toHex(hash.final());
}

bool sha256FileHex(const std::filesystem::path &path, std::string *outDigest,
                   std::string *outError) {
  if (outDigest == nullptr) {
    if (outError != nullptr) {
      *outError = "internal error: null digest output";
    }
    return false;
  }

  std::ifstream in(path, std::ios::binary);
  if (!in.is_open()) {
    if (outError != nullptr) {
      *outError = "failed to open file: " + path.string();
    }
    return false;
  }

  Sha256 hash;
  std::array<uint8_t, 4096> buffer{};
  while (in.good()) {
    in.read(reinterpret_cast<char *>(buffer.data()),
            static_cast<std::streamsize>(buffer.size()));
    const std::streamsize count = in.gcount();
    if (count > 0) {
      hash.update(buffer.data(), static_cast<size_t>(count));
    }
  }
  if (!in.eof()) {
    if (outError != nullptr) {
      *outError = "failed while reading file: " + path.string();
    }
    return false;
  }

  *outDigest = toHex(hash.final());
  return true;
}

} // namespace neuron::ncon
