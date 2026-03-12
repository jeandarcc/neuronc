#pragma once

#include "neuronc/cli/DebugSupport.h"
#include "neuronc/cli/pipeline/CompilePipeline.h"

#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace neuron::cli {

struct ProjectCommandDependencies {
  std::function<bool(neuron::ProjectConfig *, std::vector<std::string> *)>
      loadProjectConfigFromCwd;
  std::function<NeuronSettings(const std::filesystem::path &)> loadNeuronSettings;
  std::function<std::string(const std::string &, const NeuronSettings &)> readFile;
  std::function<void(const std::string &, const std::string &, const std::string &,
                     const std::vector<std::string> &)> reportStringDiagnostics;
  std::function<void(const std::string &, const std::string &,
                     const std::vector<neuron::SemanticError> &)>
      reportSemanticDiagnostics;
  std::function<void(neuron::SemanticAnalyzer *, const std::filesystem::path &,
                     const std::optional<neuron::ProjectConfig> &,
                     const NeuronSettings &, const std::string &,
                     std::vector<std::string> *)>
      configureSemanticAnalyzerModules;
  std::function<bool(const NeuronSettings &, bool, const std::string &)>
      runAutomatedTestSuite;
  std::function<int(const std::string &, std::string *)> cmdCompile;
  std::function<int(const std::string &, const CompilePipelineOptions &,
                    std::string *)>
      cmdCompileWithOptions;
  std::function<int(std::string *)> cmdPublish;
  std::function<bool(const std::filesystem::path &, const std::filesystem::path &,
                     std::string *)>
      copyOutputDllsToDirectory;
  std::function<int(const std::string &)> runSystemCommand;
  std::function<std::string(const std::filesystem::path &)> quotePath;
};

int runBuildCommand(const ProjectCommandDependencies &deps);
int runRunCommand(const ProjectCommandDependencies &deps);
int runReleaseCommand(const ProjectCommandDependencies &deps);

} // namespace neuron::cli

