// Project generator smoke tests - included from tests/test_main.cpp
#include "neuronc/cli/ProjectGenerator.h"
#include "neuronc/cli/ProjectConfig.h"

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

using namespace neuron;
namespace fs = std::filesystem;

namespace {

void removeGeneratedProjectNoThrow(const fs::path &path) {
  std::error_code ec;
  if (!fs::exists(path, ec)) {
    return;
  }
  fs::remove_all(path, ec);
}

std::string readWholeFile(const fs::path &path) {
  std::ifstream in(path, std::ios::binary);
  std::ostringstream buffer;
  buffer << in.rdbuf();
  return buffer.str();
}

} // namespace

TEST(ProjectGeneratorCreatesLibraryScaffold) {
  const std::string projectName = "tmp_lib_scaffold_test";
  const fs::path projectRoot = fs::current_path() / projectName;
  removeGeneratedProjectNoThrow(projectRoot);

  ASSERT_TRUE(ProjectGenerator::createProject(projectName, fs::current_path(),
                                              true));
  ASSERT_TRUE(fs::exists(projectRoot / "README.md"));
  ASSERT_TRUE(fs::exists(projectRoot / "LICENSE"));
  ASSERT_TRUE(fs::exists(projectRoot / "src" / "Main.npp"));

  ProjectConfig config;
  ProjectConfigParser parser;
  ASSERT_TRUE(parser.parseFile((projectRoot / "neuron.toml").string(), &config));
  ASSERT_TRUE(config.package.enabled);
  ASSERT_EQ(config.package.kind, PackageKind::Library);
  ASSERT_EQ(config.package.sourceDir, "src");
  ASSERT_EQ(config.package.license, "MIT");

  const std::string gitignore = readWholeFile(projectRoot / ".gitignore");
  ASSERT_TRUE(gitignore.find("modules/") != std::string::npos);

  const std::string settings = readWholeFile(projectRoot / ".neuronsettings");
  ASSERT_TRUE(settings.find("package_auto_add_missing = true") !=
              std::string::npos);

  const fs::path learnNeuronDoc =
      projectRoot / "agents" / "learnneuron" / "00_introduction" /
      "hello_world.md";
  ASSERT_TRUE(fs::exists(learnNeuronDoc));
  ASSERT_TRUE(readWholeFile(learnNeuronDoc).find("Hello") != std::string::npos);

  const std::string mainSource = readWholeFile(projectRoot / "src" / "Main.npp");
  ASSERT_TRUE(mainSource.find("ComputeGreeting") != std::string::npos);

  removeGeneratedProjectNoThrow(projectRoot);
  return true;
}
