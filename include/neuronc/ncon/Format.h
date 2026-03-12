#pragma once

#include "neuronc/ncon/Manifest.h"
#include "neuronc/ncon/Program.h"

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace neuron::ncon {

enum class SectionKind : uint32_t {
  ManifestJson = 1,
  Bytecode = 2,
  ResourcesIndex = 3,
  ResourcesBlob = 4,
  DebugMap = 5,
  SignatureReserved = 6,
};

struct FileHeader {
  char magic[4] = {'N', 'C', 'O', 'N'};
  uint8_t versionMajor = 1;
  uint8_t versionMinor = 0;
  uint16_t reserved16 = 0;
  uint32_t flags = 0;
  uint32_t sectionCount = 0;
  uint32_t reserved32 = 0;
};

struct SectionHeader {
  uint32_t kind = 0;
  uint64_t offset = 0;
  uint64_t size = 0;
  uint32_t crc32 = 0;
  uint32_t flags = 0;
};

struct ResourceIndexEntry {
  std::string id;
  uint64_t blobOffset = 0;
  uint64_t size = 0;
  uint32_t crc32 = 0;
};

struct ContainerData {
  std::string manifestJson;
  Program program;
  std::vector<ResourceIndexEntry> resources;
  std::vector<uint8_t> resourcesBlob;
  std::string debugMap;
};

bool writeContainer(const std::filesystem::path &outputPath,
                    const ContainerData &container, std::string *outError);
bool readContainer(const std::filesystem::path &inputPath,
                   ContainerData *outContainer, std::string *outError);
uint32_t crc32(const std::vector<uint8_t> &data);

} // namespace neuron::ncon
