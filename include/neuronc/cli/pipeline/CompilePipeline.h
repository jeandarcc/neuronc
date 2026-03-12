#pragma once

#include "neuronc/cli/DebugSupport.h"
#include "neuronc/codegen/LLVMCodeGen.h"
#include "neuronc/cli/ModuleCppSupport.h"

#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace neuron::cli {

struct CompilePipelineDependencies {
  std::function<void(const std::string &, const std::string &, const std::string &,
                     const std::vector<std::string> &)> reportStringDiagnostics;
  std::function<void(const std::string &, const std::string &,
                     const std::vector<neuron::SemanticError> &)>
      reportSemanticDiagnostics;
  std::function<NeuronSettings(const std::filesystem::path &)> loadNeuronSettings;
  std::function<std::string(const std::string &, const NeuronSettings &)> readFile;
  std::function<std::optional<neuron::ProjectConfig>()> tryLoadProjectConfigFromCwd;
  std::function<std::unordered_set<std::string>(
      const std::filesystem::path &, const std::optional<neuron::ProjectConfig> &)>
      collectAvailableModules;
  std::function<std::unordered_set<std::string>(const neuron::ProgramNode *)>
      collectImportedModuleCppModules;
  std::function<void(const std::optional<neuron::ProjectConfig> &)> applyTensorRuntimeEnv;
  std::function<neuron::LLVMOptLevel(neuron::BuildOptimizeLevel)> toLLVMOptLevel;
  std::function<neuron::LLVMTargetCPU(neuron::BuildTargetCPU)> toLLVMTargetCPU;
  std::function<const char *(neuron::BuildOptimizeLevel)> optLevelLabel;
  std::function<std::size_t(const neuron::nir::Module *)> countNIRInstructions;
  std::function<std::string()> currentHostPlatform;
  std::function<bool(const neuron::LLVMCodeGenOptions &)> ensureRuntimeObjects;
  std::function<std::filesystem::path()> runtimeObjectDirectory;
  std::function<std::filesystem::path()> runtimeSharedLibraryPath;
  std::function<std::filesystem::path()> runtimeSharedLinkPath;
  std::function<std::string(const std::filesystem::path &)> quotePath;
  std::function<std::string(const std::string &)> resolveToolCommand;
  std::function<int(const std::string &)> runSystemCommand;
  std::function<bool(const std::filesystem::path &, bool, std::string *)>
      copyBundledRuntimeDlls;
  std::function<bool(const std::filesystem::path &, const std::filesystem::path &,
                     std::string *)>
      copyFileIfExists;
  std::function<bool(const std::filesystem::path &, const neuron::LoadedModuleCppModule &,
                     const std::string &, std::filesystem::path *, std::string *)>
      buildModuleCppFromSource;
};

struct CompilePipelineOptions {
  std::string targetTripleOverride;
  bool enableWasmSimd = false;
  bool linkExecutable = true;
  std::filesystem::path outputDirOverride;
  std::filesystem::path graphicsShaderOutputDir;
  std::filesystem::path graphicsShaderCacheDir;
  bool graphicsShaderAllowCache = true;
  std::string outputStemOverride;
  std::string objectExtension = ".obj";
  std::string executableExtension = ".exe";
};

int runCompilePipeline(const std::string &filepath,
                       const CompilePipelineDependencies &deps,
                       const std::filesystem::path &toolRoot,
                       std::string *outExecutablePath = nullptr);

int runCompilePipelineWithOptions(const std::string &filepath,
                                  const CompilePipelineDependencies &deps,
                                  const std::filesystem::path &toolRoot,
                                  const CompilePipelineOptions &options,
                                  std::string *outArtifactPath = nullptr);

} // namespace neuron::cli

