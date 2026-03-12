#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace neuron::ncon {

std::string sha256Hex(const std::vector<uint8_t> &bytes);
bool sha256FileHex(const std::filesystem::path &path, std::string *outDigest,
                   std::string *outError);

} // namespace neuron::ncon
