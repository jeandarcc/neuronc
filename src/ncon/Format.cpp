#include "neuronc/ncon/Format.h"

#include <array>
#include <cstring>
#include <fstream>
#include <sstream>
#include <type_traits>

namespace neuron::ncon {

namespace {

template <typename T>
void appendScalar(std::vector<uint8_t> *out, T value) {
  static_assert(std::is_trivially_copyable_v<T>);
  const uint8_t *raw = reinterpret_cast<const uint8_t *>(&value);
  out->insert(out->end(), raw, raw + sizeof(T));
}

void appendString(std::vector<uint8_t> *out, const std::string &text) {
  appendScalar<uint32_t>(out, static_cast<uint32_t>(text.size()));
  out->insert(out->end(), text.begin(), text.end());
}

template <typename T>
bool readScalar(const std::vector<uint8_t> &data, size_t *offset, T *out) {
  static_assert(std::is_trivially_copyable_v<T>);
  if (offset == nullptr || out == nullptr ||
      *offset + sizeof(T) > data.size()) {
    return false;
  }
  std::memcpy(out, data.data() + *offset, sizeof(T));
  *offset += sizeof(T);
  return true;
}

bool readString(const std::vector<uint8_t> &data, size_t *offset,
                std::string *out) {
  uint32_t size = 0;
  if (!readScalar<uint32_t>(data, offset, &size)) {
    return false;
  }
  if (offset == nullptr || out == nullptr || *offset + size > data.size()) {
    return false;
  }
  out->assign(reinterpret_cast<const char *>(data.data() + *offset), size);
  *offset += size;
  return true;
}

uint64_t align8(uint64_t value) {
  return (value + 7u) & ~uint64_t{7u};
}

std::vector<uint8_t> serializeProgram(const Program &program) {
  std::vector<uint8_t> out;
  out.push_back('N');
  out.push_back('C');
  out.push_back('I');
  out.push_back('R');
  appendScalar<uint16_t>(&out, 1);
  appendScalar<uint16_t>(&out, 0);
  appendScalar<uint32_t>(&out, static_cast<uint32_t>(program.strings.size()));
  appendScalar<uint32_t>(&out, static_cast<uint32_t>(program.types.size()));
  appendScalar<uint32_t>(&out, static_cast<uint32_t>(program.constants.size()));
  appendScalar<uint32_t>(&out, static_cast<uint32_t>(program.globals.size()));
  appendScalar<uint32_t>(&out, static_cast<uint32_t>(program.functions.size()));
  appendScalar<uint32_t>(&out, static_cast<uint32_t>(program.blocks.size()));
  appendScalar<uint32_t>(&out,
                         static_cast<uint32_t>(program.instructions.size()));
  appendScalar<uint32_t>(&out, static_cast<uint32_t>(program.operands.size()));
  appendScalar<uint32_t>(&out, program.entryFunctionId);
  appendString(&out, program.moduleName);

  for (const std::string &text : program.strings) {
    appendString(&out, text);
  }

  for (const TypeRecord &record : program.types) {
    appendScalar<uint32_t>(&out, static_cast<uint32_t>(record.kind));
    appendScalar<uint32_t>(&out, record.nameStringId);
    appendScalar<uint32_t>(&out, record.pointeeTypeId);
    appendScalar<uint32_t>(&out, record.returnTypeId);
    appendScalar<uint32_t>(&out,
                           static_cast<uint32_t>(record.genericTypeIds.size()));
    appendScalar<uint32_t>(&out,
                           static_cast<uint32_t>(record.paramTypeIds.size()));
    appendScalar<uint32_t>(&out,
                           static_cast<uint32_t>(record.fieldTypeIds.size()));
    for (uint32_t value : record.genericTypeIds) {
      appendScalar<uint32_t>(&out, value);
    }
    for (uint32_t value : record.paramTypeIds) {
      appendScalar<uint32_t>(&out, value);
    }
    for (size_t i = 0; i < record.fieldTypeIds.size(); ++i) {
      appendScalar<uint32_t>(&out, record.fieldNameStringIds[i]);
      appendScalar<uint32_t>(&out, record.fieldTypeIds[i]);
    }
  }

  for (const ConstantRecord &record : program.constants) {
    appendScalar<uint32_t>(&out, static_cast<uint32_t>(record.kind));
    appendScalar<uint32_t>(&out, record.typeId);
    appendScalar<int64_t>(&out, record.intValue);
    appendScalar<double>(&out, record.floatValue);
    appendScalar<uint32_t>(&out, record.stringId);
  }

  for (const GlobalRecord &record : program.globals) {
    appendScalar<uint32_t>(&out, record.nameStringId);
    appendScalar<uint32_t>(&out, record.typeId);
    appendScalar<uint32_t>(&out, record.initializerConstantId);
  }

  for (const FunctionRecord &record : program.functions) {
    appendScalar<uint32_t>(&out, record.nameStringId);
    appendScalar<uint32_t>(&out, record.returnTypeId);
    appendScalar<uint32_t>(&out, record.blockBegin);
    appendScalar<uint32_t>(&out, record.blockCount);
    appendScalar<uint32_t>(&out, record.argCount);
    appendScalar<uint32_t>(&out, record.slotCount);
    appendScalar<uint32_t>(&out, record.flags);
    appendScalar<uint32_t>(&out,
                           static_cast<uint32_t>(record.argTypeIds.size()));
    for (uint32_t argTypeId : record.argTypeIds) {
      appendScalar<uint32_t>(&out, argTypeId);
    }
  }

  for (const BlockRecord &record : program.blocks) {
    appendScalar<uint32_t>(&out, record.nameStringId);
    appendScalar<uint32_t>(&out, record.instructionBegin);
    appendScalar<uint32_t>(&out, record.instructionCount);
  }

  for (const InstructionRecord &record : program.instructions) {
    appendScalar<uint16_t>(&out, record.opcode);
    appendScalar<uint16_t>(&out, record.flags);
    appendScalar<uint32_t>(&out, record.dstSlot);
    appendScalar<uint32_t>(&out, record.typeId);
    appendScalar<uint32_t>(&out, record.operandBegin);
    appendScalar<uint32_t>(&out, record.operandCount);
    appendScalar<uint32_t>(&out, record.imm0);
    appendScalar<uint32_t>(&out, record.imm1);
  }

  for (const OperandRecord &record : program.operands) {
    appendScalar<uint16_t>(&out, static_cast<uint16_t>(record.kind));
    appendScalar<uint16_t>(&out, 0);
    appendScalar<uint32_t>(&out, record.value);
  }
  return out;
}

bool deserializeProgram(const std::vector<uint8_t> &data, Program *out,
                        std::string *outError) {
  if (out == nullptr) {
    if (outError != nullptr) {
      *outError = "internal error: null bytecode output";
    }
    return false;
  }

  size_t offset = 0;
  if (data.size() < 4) {
    if (outError != nullptr) {
      *outError = "truncated NCIR header";
    }
    return false;
  }
  char magic[4];
  std::memcpy(magic, data.data(), sizeof(magic));
  offset += sizeof(magic);
  if (std::memcmp(magic, "NCIR", 4) != 0) {
    if (outError != nullptr) {
      *outError = "invalid NCIR magic";
    }
    return false;
  }

  uint16_t versionMajor = 0;
  uint16_t versionMinor = 0;
  uint32_t stringCount = 0;
  uint32_t typeCount = 0;
  uint32_t constantCount = 0;
  uint32_t globalCount = 0;
  uint32_t functionCount = 0;
  uint32_t blockCount = 0;
  uint32_t instructionCount = 0;
  uint32_t operandCount = 0;
  if (!readScalar<uint16_t>(data, &offset, &versionMajor) ||
      !readScalar<uint16_t>(data, &offset, &versionMinor) ||
      !readScalar<uint32_t>(data, &offset, &stringCount) ||
      !readScalar<uint32_t>(data, &offset, &typeCount) ||
      !readScalar<uint32_t>(data, &offset, &constantCount) ||
      !readScalar<uint32_t>(data, &offset, &globalCount) ||
      !readScalar<uint32_t>(data, &offset, &functionCount) ||
      !readScalar<uint32_t>(data, &offset, &blockCount) ||
      !readScalar<uint32_t>(data, &offset, &instructionCount) ||
      !readScalar<uint32_t>(data, &offset, &operandCount) ||
      !readScalar<uint32_t>(data, &offset, &out->entryFunctionId) ||
      !readString(data, &offset, &out->moduleName)) {
    if (outError != nullptr) {
      *outError = "truncated NCIR header";
    }
    return false;
  }
  if (versionMajor != 1 || versionMinor != 0) {
    if (outError != nullptr) {
      *outError = "unsupported NCIR version";
    }
    return false;
  }

  out->strings.resize(stringCount);
  for (std::string &text : out->strings) {
    if (!readString(data, &offset, &text)) {
      if (outError != nullptr) {
        *outError = "truncated NCIR string table";
      }
      return false;
    }
  }

  out->types.resize(typeCount);
  for (TypeRecord &record : out->types) {
    uint32_t kind = 0;
    uint32_t genericCount = 0;
    uint32_t paramCount = 0;
    uint32_t fieldCount = 0;
    if (!readScalar<uint32_t>(data, &offset, &kind) ||
        !readScalar<uint32_t>(data, &offset, &record.nameStringId) ||
        !readScalar<uint32_t>(data, &offset, &record.pointeeTypeId) ||
        !readScalar<uint32_t>(data, &offset, &record.returnTypeId) ||
        !readScalar<uint32_t>(data, &offset, &genericCount) ||
        !readScalar<uint32_t>(data, &offset, &paramCount) ||
        !readScalar<uint32_t>(data, &offset, &fieldCount)) {
      if (outError != nullptr) {
        *outError = "truncated NCIR type table";
      }
      return false;
    }
    record.kind = static_cast<TypeKind>(kind);
    record.genericTypeIds.resize(genericCount);
    record.paramTypeIds.resize(paramCount);
    record.fieldNameStringIds.resize(fieldCount);
    record.fieldTypeIds.resize(fieldCount);
    for (uint32_t &value : record.genericTypeIds) {
      if (!readScalar<uint32_t>(data, &offset, &value)) {
        if (outError != nullptr) {
          *outError = "truncated NCIR type generics";
        }
        return false;
      }
    }
    for (uint32_t &value : record.paramTypeIds) {
      if (!readScalar<uint32_t>(data, &offset, &value)) {
        if (outError != nullptr) {
          *outError = "truncated NCIR type params";
        }
        return false;
      }
    }
    for (uint32_t i = 0; i < fieldCount; ++i) {
      if (!readScalar<uint32_t>(data, &offset, &record.fieldNameStringIds[i]) ||
          !readScalar<uint32_t>(data, &offset, &record.fieldTypeIds[i])) {
        if (outError != nullptr) {
          *outError = "truncated NCIR type fields";
        }
        return false;
      }
    }
  }

  out->constants.resize(constantCount);
  for (ConstantRecord &record : out->constants) {
    uint32_t kind = 0;
    if (!readScalar<uint32_t>(data, &offset, &kind) ||
        !readScalar<uint32_t>(data, &offset, &record.typeId) ||
        !readScalar<int64_t>(data, &offset, &record.intValue) ||
        !readScalar<double>(data, &offset, &record.floatValue) ||
        !readScalar<uint32_t>(data, &offset, &record.stringId)) {
      if (outError != nullptr) {
        *outError = "truncated NCIR constant table";
      }
      return false;
    }
    record.kind = static_cast<ConstantKind>(kind);
  }

  out->globals.resize(globalCount);
  for (GlobalRecord &record : out->globals) {
    if (!readScalar<uint32_t>(data, &offset, &record.nameStringId) ||
        !readScalar<uint32_t>(data, &offset, &record.typeId) ||
        !readScalar<uint32_t>(data, &offset, &record.initializerConstantId)) {
      if (outError != nullptr) {
        *outError = "truncated NCIR global table";
      }
      return false;
    }
  }

  out->functions.resize(functionCount);
  for (FunctionRecord &record : out->functions) {
    uint32_t argTypeCount = 0;
    if (!readScalar<uint32_t>(data, &offset, &record.nameStringId) ||
        !readScalar<uint32_t>(data, &offset, &record.returnTypeId) ||
        !readScalar<uint32_t>(data, &offset, &record.blockBegin) ||
        !readScalar<uint32_t>(data, &offset, &record.blockCount) ||
        !readScalar<uint32_t>(data, &offset, &record.argCount) ||
        !readScalar<uint32_t>(data, &offset, &record.slotCount) ||
        !readScalar<uint32_t>(data, &offset, &record.flags) ||
        !readScalar<uint32_t>(data, &offset, &argTypeCount)) {
      if (outError != nullptr) {
        *outError = "truncated NCIR function table";
      }
      return false;
    }
    record.argTypeIds.resize(argTypeCount);
    for (uint32_t &argTypeId : record.argTypeIds) {
      if (!readScalar<uint32_t>(data, &offset, &argTypeId)) {
        if (outError != nullptr) {
          *outError = "truncated NCIR function arg table";
        }
        return false;
      }
    }
  }

  out->blocks.resize(blockCount);
  for (BlockRecord &record : out->blocks) {
    if (!readScalar<uint32_t>(data, &offset, &record.nameStringId) ||
        !readScalar<uint32_t>(data, &offset, &record.instructionBegin) ||
        !readScalar<uint32_t>(data, &offset, &record.instructionCount)) {
      if (outError != nullptr) {
        *outError = "truncated NCIR block table";
      }
      return false;
    }
  }

  out->instructions.resize(instructionCount);
  for (InstructionRecord &record : out->instructions) {
    if (!readScalar<uint16_t>(data, &offset, &record.opcode) ||
        !readScalar<uint16_t>(data, &offset, &record.flags) ||
        !readScalar<uint32_t>(data, &offset, &record.dstSlot) ||
        !readScalar<uint32_t>(data, &offset, &record.typeId) ||
        !readScalar<uint32_t>(data, &offset, &record.operandBegin) ||
        !readScalar<uint32_t>(data, &offset, &record.operandCount) ||
        !readScalar<uint32_t>(data, &offset, &record.imm0) ||
        !readScalar<uint32_t>(data, &offset, &record.imm1)) {
      if (outError != nullptr) {
        *outError = "truncated NCIR instruction table";
      }
      return false;
    }
  }

  out->operands.resize(operandCount);
  for (OperandRecord &record : out->operands) {
    uint16_t kind = 0;
    uint16_t ignored = 0;
    if (!readScalar<uint16_t>(data, &offset, &kind) ||
        !readScalar<uint16_t>(data, &offset, &ignored) ||
        !readScalar<uint32_t>(data, &offset, &record.value)) {
      if (outError != nullptr) {
        *outError = "truncated NCIR operand table";
      }
      return false;
    }
    record.kind = static_cast<OperandKind>(kind);
  }
  return true;
}

std::vector<uint8_t> serializeResourceIndex(
    const std::vector<ResourceIndexEntry> &resources) {
  std::vector<uint8_t> out;
  appendScalar<uint32_t>(&out, static_cast<uint32_t>(resources.size()));
  for (const ResourceIndexEntry &resource : resources) {
    appendString(&out, resource.id);
    appendScalar<uint64_t>(&out, resource.blobOffset);
    appendScalar<uint64_t>(&out, resource.size);
    appendScalar<uint32_t>(&out, resource.crc32);
  }
  return out;
}

bool deserializeResourceIndex(const std::vector<uint8_t> &data,
                              std::vector<ResourceIndexEntry> *outResources,
                              std::string *outError) {
  if (outResources == nullptr) {
    if (outError != nullptr) {
      *outError = "internal error: null resource index output";
    }
    return false;
  }
  outResources->clear();
  size_t offset = 0;
  uint32_t count = 0;
  if (!readScalar<uint32_t>(data, &offset, &count)) {
    if (outError != nullptr) {
      *outError = "truncated resource index";
    }
    return false;
  }
  outResources->resize(count);
  for (ResourceIndexEntry &resource : *outResources) {
    if (!readString(data, &offset, &resource.id) ||
        !readScalar<uint64_t>(data, &offset, &resource.blobOffset) ||
        !readScalar<uint64_t>(data, &offset, &resource.size) ||
        !readScalar<uint32_t>(data, &offset, &resource.crc32)) {
      if (outError != nullptr) {
        *outError = "truncated resource index";
      }
      return false;
    }
  }
  return true;
}

std::vector<uint8_t> stringToBytes(const std::string &text) {
  return std::vector<uint8_t>(text.begin(), text.end());
}

std::array<uint32_t, 256> makeCrc32Table() {
  std::array<uint32_t, 256> table{};
  for (uint32_t i = 0; i < table.size(); ++i) {
    uint32_t value = i;
    for (int bit = 0; bit < 8; ++bit) {
      value = (value & 1u) != 0u ? (0xEDB88320u ^ (value >> 1u))
                                 : (value >> 1u);
    }
    table[i] = value;
  }
  return table;
}

} // namespace

uint32_t crc32(const std::vector<uint8_t> &data) {
  static const std::array<uint32_t, 256> table = makeCrc32Table();
  uint32_t crc = 0xFFFFFFFFu;
  for (uint8_t byte : data) {
    crc = table[(crc ^ byte) & 0xFFu] ^ (crc >> 8u);
  }
  return crc ^ 0xFFFFFFFFu;
}

bool writeContainer(const std::filesystem::path &outputPath,
                    const ContainerData &container, std::string *outError) {
  const std::vector<uint8_t> manifestBytes = stringToBytes(container.manifestJson);
  const std::vector<uint8_t> bytecodeBytes = serializeProgram(container.program);
  const std::vector<uint8_t> resourcesIndexBytes =
      serializeResourceIndex(container.resources);
  const std::vector<uint8_t> resourcesBlobBytes = container.resourcesBlob;
  const std::vector<uint8_t> debugMapBytes = stringToBytes(container.debugMap);

  struct PendingSection {
    SectionKind kind;
    std::vector<uint8_t> bytes;
  };

  std::vector<PendingSection> sections = {
      {SectionKind::ManifestJson, manifestBytes},
      {SectionKind::Bytecode, bytecodeBytes},
      {SectionKind::ResourcesIndex, resourcesIndexBytes},
      {SectionKind::ResourcesBlob, resourcesBlobBytes},
      {SectionKind::DebugMap, debugMapBytes},
  };

  FileHeader header;
  header.sectionCount = static_cast<uint32_t>(sections.size());
  std::vector<SectionHeader> sectionHeaders(sections.size());

  uint64_t cursor = sizeof(FileHeader) +
                    sizeof(SectionHeader) * static_cast<uint64_t>(sections.size());
  cursor = align8(cursor);
  for (size_t i = 0; i < sections.size(); ++i) {
    sectionHeaders[i].kind = static_cast<uint32_t>(sections[i].kind);
    sectionHeaders[i].offset = cursor;
    sectionHeaders[i].size = sections[i].bytes.size();
    sectionHeaders[i].crc32 = crc32(sections[i].bytes);
    cursor = align8(cursor + sectionHeaders[i].size);
  }

  std::error_code ec;
  std::filesystem::create_directories(outputPath.parent_path(), ec);
  std::ofstream out(outputPath, std::ios::binary | std::ios::trunc);
  if (!out.is_open()) {
    if (outError != nullptr) {
      *outError = "failed to open output container: " + outputPath.string();
    }
    return false;
  }

  out.write(reinterpret_cast<const char *>(&header), sizeof(header));
  out.write(reinterpret_cast<const char *>(sectionHeaders.data()),
            static_cast<std::streamsize>(sectionHeaders.size() *
                                         sizeof(SectionHeader)));
  const uint64_t writtenHeaderBytes =
      sizeof(FileHeader) +
      sizeof(SectionHeader) * static_cast<uint64_t>(sectionHeaders.size());
  const uint64_t paddingBytes = align8(writtenHeaderBytes) - writtenHeaderBytes;
  for (uint64_t i = 0; i < paddingBytes; ++i) {
    out.put('\0');
  }

  for (size_t i = 0; i < sections.size(); ++i) {
    const auto currentPos = static_cast<uint64_t>(out.tellp());
    if (currentPos < sectionHeaders[i].offset) {
      const uint64_t sectionPad = sectionHeaders[i].offset - currentPos;
      for (uint64_t j = 0; j < sectionPad; ++j) {
        out.put('\0');
      }
    }
    if (!sections[i].bytes.empty()) {
      out.write(reinterpret_cast<const char *>(sections[i].bytes.data()),
                static_cast<std::streamsize>(sections[i].bytes.size()));
    }
    const uint64_t endPos = static_cast<uint64_t>(out.tellp());
    const uint64_t alignedEnd = align8(endPos);
    for (uint64_t j = endPos; j < alignedEnd; ++j) {
      out.put('\0');
    }
  }

  if (!out.good()) {
    if (outError != nullptr) {
      *outError = "failed while writing output container";
    }
    return false;
  }
  return true;
}

bool readContainer(const std::filesystem::path &inputPath,
                   ContainerData *outContainer, std::string *outError) {
  if (outContainer == nullptr) {
    if (outError != nullptr) {
      *outError = "internal error: null container output";
    }
    return false;
  }

  std::ifstream in(inputPath, std::ios::binary);
  if (!in.is_open()) {
    if (outError != nullptr) {
      *outError = "failed to open container: " + inputPath.string();
    }
    return false;
  }

  FileHeader header;
  in.read(reinterpret_cast<char *>(&header), sizeof(header));
  if (in.gcount() != static_cast<std::streamsize>(sizeof(header))) {
    if (outError != nullptr) {
      *outError = "truncated NCON header";
    }
    return false;
  }

  if (std::memcmp(header.magic, "NCON", 4) != 0) {
    if (outError != nullptr) {
      *outError = "invalid NCON magic";
    }
    return false;
  }
  if (header.versionMajor != 1 || header.versionMinor != 0) {
    if (outError != nullptr) {
      *outError = "unsupported NCON version";
    }
    return false;
  }

  std::vector<SectionHeader> sectionHeaders(header.sectionCount);
  if (!sectionHeaders.empty()) {
    in.read(reinterpret_cast<char *>(sectionHeaders.data()),
            static_cast<std::streamsize>(sectionHeaders.size() *
                                         sizeof(SectionHeader)));
    if (!in.good()) {
      if (outError != nullptr) {
        *outError = "truncated NCON section header table";
      }
      return false;
    }
  }

  outContainer->manifestJson.clear();
  outContainer->program = Program{};
  outContainer->resources.clear();
  outContainer->resourcesBlob.clear();
  outContainer->debugMap.clear();

  for (const SectionHeader &section : sectionHeaders) {
    in.seekg(static_cast<std::streamoff>(section.offset), std::ios::beg);
    std::vector<uint8_t> bytes(static_cast<size_t>(section.size));
    if (!bytes.empty()) {
      in.read(reinterpret_cast<char *>(bytes.data()),
              static_cast<std::streamsize>(bytes.size()));
      if (in.gcount() != static_cast<std::streamsize>(bytes.size())) {
        if (outError != nullptr) {
          *outError = "truncated NCON section payload";
        }
        return false;
      }
    }
    if (crc32(bytes) != section.crc32) {
      if (outError != nullptr) {
        *outError = "NCON section CRC mismatch";
      }
      return false;
    }

    switch (static_cast<SectionKind>(section.kind)) {
    case SectionKind::ManifestJson:
      outContainer->manifestJson.assign(bytes.begin(), bytes.end());
      break;
    case SectionKind::Bytecode:
      if (!deserializeProgram(bytes, &outContainer->program, outError)) {
        return false;
      }
      break;
    case SectionKind::ResourcesIndex:
      if (!deserializeResourceIndex(bytes, &outContainer->resources,
                                    outError)) {
        return false;
      }
      break;
    case SectionKind::ResourcesBlob:
      outContainer->resourcesBlob = std::move(bytes);
      break;
    case SectionKind::DebugMap:
      outContainer->debugMap.assign(bytes.begin(), bytes.end());
      break;
    case SectionKind::SignatureReserved:
      break;
    }
  }

  return true;
}

} // namespace neuron::ncon
