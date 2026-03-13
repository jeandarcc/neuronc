// Project command tests - included from tests/test_main.cpp
#include "neuronc/cli/commands/ProjectCommands.h"

#include <filesystem>
#include <fstream>
#include <optional>
#include <string>

using namespace neuron;
using namespace neuron::cli;
namespace fs = std::filesystem;

namespace {

void removeProjectCommandTempDir(const fs::path &path) {
  std::error_code ec;
  if (!fs::exists(path, ec)) {
    return;
  }
  fs::remove_all(path, ec);
}

} // namespace

TEST(BuildCommandCompilesIntoDevOutputDirectory) {
  const fs::path tempDir = fs::current_path() / "tmp_project_command_build";
  removeProjectCommandTempDir(tempDir);
  fs::create_directories(tempDir / "src");

  {
    std::ofstream mainFile(tempDir / "src" / "Main.nr");
    mainFile << "Init is method() { Print(\"ok\"); }\n";
  }

  ProjectConfig config;
  config.name = "sample_app";
  config.buildDir = "build";
  config.mainFile = (tempDir / "src" / "Main.nr").string();

  bool testsRan = false;
  bool compileCalled = false;
  std::optional<CompilePipelineOptions> capturedOptions;
  std::string capturedFile;

  ProjectCommandDependencies deps;
  deps.loadProjectConfigFromCwd =
      [&](ProjectConfig *outConfig, std::vector<std::string> *) {
        *outConfig = config;
        return true;
      };
  deps.loadNeuronSettings = [](const fs::path &) { return NeuronSettings(); };
  deps.runAutomatedTestSuite =
      [&](const NeuronSettings &, bool debugMode, const std::string &label) {
        testsRan = true;
        ASSERT_TRUE(debugMode);
        ASSERT_EQ(label, "Build");
        return true;
      };
  deps.cmdCompileWithOptions =
      [&](const std::string &filePath, const CompilePipelineOptions &options,
          std::string *outArtifactPath) {
        compileCalled = true;
        capturedFile = filePath;
        capturedOptions = options;
        if (outArtifactPath != nullptr) {
          *outArtifactPath =
              (fs::path("build") / "dev" / "sample_app.exe").string();
        }
        return 0;
      };

  ASSERT_EQ(runBuildCommand(deps), 0);
  ASSERT_TRUE(testsRan);
  ASSERT_TRUE(compileCalled);
  ASSERT_EQ(capturedFile, config.mainFile);
  ASSERT_TRUE(capturedOptions.has_value());
  ASSERT_EQ(capturedOptions->outputDirOverride, fs::path("build") / "dev");
  ASSERT_EQ(capturedOptions->outputStemOverride, "sample_app");

  removeProjectCommandTempDir(tempDir);
  return true;
}
