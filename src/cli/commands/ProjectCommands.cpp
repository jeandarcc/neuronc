#include "neuronc/cli/commands/ProjectCommands.h"

#include <filesystem>
#include <fstream>
#include <iostream>

namespace fs = std::filesystem;

namespace neuron::cli {

int runBuildCommand(const ProjectCommandDependencies &deps) {
  neuron::ProjectConfig config;
  std::vector<std::string> errors;
  if (!deps.loadProjectConfigFromCwd(&config, &errors)) {
    for (const auto &error : errors) {
      std::cerr << error << std::endl;
    }
    return 1;
  }

  const std::string mainFile =
      config.mainFile.empty() ? "src/Main.npp" : config.mainFile;
  if (!fs::exists(mainFile)) {
    std::cerr << "Error: Entry file not found: " << mainFile << std::endl;
    return 1;
  }

  const NeuronSettings settings = deps.loadNeuronSettings(fs::path(mainFile));
  if (!deps.runAutomatedTestSuite(settings, true, "Build")) {
    return 1;
  }

  CompilePipelineOptions options;
  const fs::path buildRoot =
      fs::path(config.buildDir.empty() ? "build" : config.buildDir);
  options.outputDirOverride = buildRoot / "dev";
  options.outputStemOverride =
      config.name.empty() ? fs::path(mainFile).stem().string() : config.name;

  std::string artifactPath;
  const int compileResult =
      deps.cmdCompileWithOptions(mainFile, options, &artifactPath);
  if (compileResult != 0) {
    return compileResult;
  }

  if (!artifactPath.empty()) {
    std::cout << "Dev build ready: " << artifactPath << std::endl;
  }
  return 0;
}

int runRunCommand(const ProjectCommandDependencies &deps) {
  neuron::ProjectConfig config;
  std::vector<std::string> errors;
  if (!deps.loadProjectConfigFromCwd(&config, &errors)) {
    for (const auto &e : errors) {
      std::cerr << e << std::endl;
    }
    return 1;
  }

  const std::string mainFile =
      config.mainFile.empty() ? "src/Main.npp" : config.mainFile;
  std::string executablePath;
  const int compileResult = deps.cmdCompile(mainFile, &executablePath);
  if (compileResult != 0) {
    return compileResult;
  }

  fs::path exePath(executablePath);
  if (!fs::exists(exePath)) {
    std::cerr << "Error: expected executable not found: " << exePath.string()
              << std::endl;
    return 1;
  }

  const std::string runCmd = deps.quotePath(exePath);
  const int runResult = deps.runSystemCommand(runCmd);
  if (runResult != 0) {
    std::cerr << "Program exited with non-zero code: " << runResult
              << std::endl;
    return 1;
  }

  return 0;
}

int runReleaseCommand(const ProjectCommandDependencies &deps) {
  neuron::ProjectConfig config;
  std::vector<std::string> errors;
  if (!deps.loadProjectConfigFromCwd(&config, &errors)) {
    for (const auto &e : errors) {
      std::cerr << e << std::endl;
    }
    return 1;
  }

  const std::string mainFile =
      config.mainFile.empty() ? "src/Main.npp" : config.mainFile;
  const NeuronSettings settings = deps.loadNeuronSettings(fs::path(mainFile));
  if (!deps.runAutomatedTestSuite(settings, false, "Release")) {
    return 1;
  }

  std::string executablePath;
  const int compileResult = deps.cmdCompile(mainFile, &executablePath);
  if (compileResult != 0) {
    return compileResult;
  }

  fs::path exePath(executablePath);
  if (!fs::exists(exePath)) {
    std::cerr << "Release blocked: executable was not produced ("
              << exePath.string() << ")." << std::endl;
    return 1;
  }

  fs::path releaseDir =
      fs::path(config.buildDir.empty() ? "build" : config.buildDir) /
      "release" / (config.name + "-" + config.version);
  std::error_code ec;
  fs::create_directories(releaseDir, ec);
  if (ec) {
    std::cerr << "Failed to create release directory: " << ec.message()
              << std::endl;
    return 1;
  }

  fs::copy_file(exePath, releaseDir / (config.name + ".exe"),
                fs::copy_options::overwrite_existing, ec);
  if (ec) {
    std::cerr << "Failed to copy executable to release directory: "
              << ec.message() << std::endl;
    return 1;
  }

  std::string dllCopyError;
  if (!deps.copyOutputDllsToDirectory(exePath.parent_path(), releaseDir,
                                      &dllCopyError)) {
    std::cerr << dllCopyError << std::endl;
    return 1;
  }

  fs::copy_file("neuron.toml", releaseDir / "neuron.toml",
                fs::copy_options::overwrite_existing, ec);
  if (ec) {
    std::cerr << "Failed to copy neuron.toml: " << ec.message() << std::endl;
    return 1;
  }

  std::string packageArtifact;
  if (deps.cmdPublish(&packageArtifact) != 0) {
    return 1;
  }

  if (!packageArtifact.empty() && fs::exists(packageArtifact)) {
    fs::copy_file(packageArtifact,
                  releaseDir / fs::path(packageArtifact).filename(),
                  fs::copy_options::overwrite_existing, ec);
    if (ec) {
      std::cerr << "Failed to copy package artifact into release directory: "
                << ec.message() << std::endl;
      return 1;
    }
  }

  std::ofstream notes(releaseDir / "RELEASE_NOTES.txt");
  notes << "Neuron++ Release Bundle\n";
  notes << "Project: " << config.name << "\n";
  notes << "Version: " << config.version << "\n";
  notes << "Entry: " << mainFile << "\n";
  notes << "Artifacts:\n";
  notes << "  - " << (config.name + ".exe") << "\n";
  for (const auto &entry : fs::directory_iterator(releaseDir)) {
    if (!entry.is_regular_file() || entry.path().extension() != ".dll") {
      continue;
    }
    notes << "  - " << entry.path().filename().string() << "\n";
  }
  if (!packageArtifact.empty()) {
    notes << "  - " << fs::path(packageArtifact).filename().string() << "\n";
  }
  notes << "  - neuron.toml\n";

  std::cout << "Release bundle ready at: " << releaseDir.string() << std::endl;
  return 0;
}

} // namespace neuron::cli

