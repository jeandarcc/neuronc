#pragma once

#include "neuronc/cli/ProjectConfig.h"
#include "neuronc/sema/SemanticAnalyzer.h"

#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace neuron::cli {

struct NeuronSettings {
  int maxLinesPerFile = 1000;
  int maxClassesPerFile = 1;
  bool requireMethodUppercaseStart = true;
  bool enforceStrictFileNaming = true;
  bool forbidRootScripts = true;
  int maxLinesPerMethod = 50;
  int maxLinesPerBlockStatement = 20;
  int minMethodNameLength = 4;
  bool requireClassExplicitVisibility = true;
  bool requirePropertyExplicitVisibility = true;
  bool requireConstUppercase = true;
  int maxNestingDepth = 3;
  bool requireScriptDocs = true;
  std::vector<std::string> requireScriptDocsExclude = {"Test*"};
  int requireScriptDocsMinLines = 5;
  int maxAutoTestDurationMs = 5000;
  bool requirePublicMethodDocs = true;
  bool packageAutoAddMissing = true;
  bool agentHints = true;
  std::filesystem::path settingsRoot;
};

struct DebugCommandDependencies {
  std::function<void(const std::string &, const std::string &, const std::string &,
                     const std::vector<std::string> &)> reportStringDiagnostics;
  std::function<void(const std::string &, const std::string &,
                     const std::vector<neuron::SemanticError> &)>
      reportSemanticDiagnostics;
  std::function<std::optional<neuron::ProjectConfig>()> tryLoadProjectConfigFromCwd;
  std::function<void(neuron::SemanticAnalyzer *, const std::filesystem::path &,
                     const std::optional<neuron::ProjectConfig> &,
                     const NeuronSettings &, const std::string &,
                     std::vector<std::string> *)>
      configureSemanticAnalyzerModules;
  std::filesystem::path toolRoot;
  std::function<NeuronSettings(const std::filesystem::path &)> loadNeuronSettings;
  std::function<std::string(const std::string &, const NeuronSettings &)> readFile;
};

} // namespace neuron::cli

