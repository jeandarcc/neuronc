#include "neuronc/ncon/Format.h"

#include <filesystem>
#include <fstream>

using namespace neuron;
namespace fs = std::filesystem;

static ncon::ContainerData makeFormatTestContainer() {
  ncon::ContainerData container;
  container.manifestJson = "{\"format\":\"ncon-v1\"}\n";
  container.program.moduleName = "format_demo";
  container.program.entryFunctionId = 0;
  container.program.strings = {"Init"};
  container.program.types.push_back(
      {TypeKind::Void, ncon::kInvalidIndex, ncon::kInvalidIndex, ncon::kInvalidIndex});
  container.program.functions.push_back({0, 0, 0, 0, 0, 0, 0, {}});
  container.debugMap = "debug\n";
  return container;
}

TEST(NconFormatRoundTrip) {
  const fs::path path = fs::current_path() / "tmp_ncon_roundtrip.ncon";
  fs::remove(path);

  ncon::ContainerData input = makeFormatTestContainer();
  std::string error;
  ASSERT_TRUE(ncon::writeContainer(path, input, &error));
  ASSERT_TRUE(error.empty());

  ncon::ContainerData output;
  ASSERT_TRUE(ncon::readContainer(path, &output, &error));
  ASSERT_EQ(output.manifestJson, input.manifestJson);
  ASSERT_EQ(output.program.moduleName, input.program.moduleName);
  ASSERT_EQ(output.debugMap, input.debugMap);

  fs::remove(path);
  return true;
}

TEST(NconFormatRejectsCorruptSection) {
  const fs::path path = fs::current_path() / "tmp_ncon_corrupt.ncon";
  fs::remove(path);

  ncon::ContainerData input = makeFormatTestContainer();
  std::string error;
  ASSERT_TRUE(ncon::writeContainer(path, input, &error));

  std::fstream file(path, std::ios::binary | std::ios::in | std::ios::out);
  ncon::FileHeader header{};
  ncon::SectionHeader firstSection{};
  file.read(reinterpret_cast<char *>(&header), sizeof(header));
  file.read(reinterpret_cast<char *>(&firstSection), sizeof(firstSection));
  file.seekg(static_cast<std::streamoff>(firstSection.offset), std::ios::beg);
  char byte = '\0';
  file.read(&byte, 1);
  file.seekp(static_cast<std::streamoff>(firstSection.offset), std::ios::beg);
  byte = static_cast<char>(byte ^ 0x7f);
  file.write(&byte, 1);
  file.close();

  ncon::ContainerData output;
  ASSERT_FALSE(ncon::readContainer(path, &output, &error));
  if (error.find("CRC mismatch") == std::string::npos) {
    std::cerr << error << std::endl;
  }
  ASSERT_TRUE(error.find("CRC mismatch") != std::string::npos);

  fs::remove(path);
  return true;
}
