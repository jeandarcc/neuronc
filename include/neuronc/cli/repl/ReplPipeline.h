#pragma once

#include "neuronc/cli/DebugSupport.h"
#include "neuronc/cli/ProjectConfig.h"
#include "neuronc/frontend/Diagnostics.h"
#include "neuronc/codegen/LLVMCodeGen.h"
#include "neuronc/sema/SemanticAnalyzer.h"

#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace neuron {
class JITEngine;
}

namespace neuron::cli {

struct ReplCell {
  std::string virtualPath;
  std::string source;
};

class ReplSession {
public:
  void reset();
  void commit(std::string virtualPath, std::string source);

  const std::vector<ReplCell> &cells() const { return m_cells; }
  std::size_t nextCellIndex() const { return m_cells.size() + 1; }

private:
  std::vector<ReplCell> m_cells;
};

struct ReplPipelineDependencies {
  std::function<NeuronSettings(const std::filesystem::path &)> loadNeuronSettings;
  std::function<std::optional<neuron::ProjectConfig>()> tryLoadProjectConfigFromCwd;
  std::function<std::unordered_set<std::string>(
      const std::filesystem::path &, const std::optional<neuron::ProjectConfig> &)>
      collectAvailableModules;
  std::function<void(const std::optional<neuron::ProjectConfig> &)>
      applyTensorRuntimeEnv;
  std::function<neuron::LLVMOptLevel(neuron::BuildOptimizeLevel)> toLLVMOptLevel;
  std::function<neuron::LLVMTargetCPU(neuron::BuildTargetCPU)> toLLVMTargetCPU;
  std::function<bool(const neuron::LLVMCodeGenOptions &)> ensureRuntimeObjects;
  std::function<std::filesystem::path()> runtimeSharedLibraryPath;
  std::function<bool(neuron::JITEngine *, std::string *)> initializeJitEngine;
};

struct ReplSubmitResult {
  bool committed = false;
  std::string phase;
  std::string runtimeError;
  std::vector<neuron::frontend::Diagnostic> diagnostics;
  std::vector<std::string> stringDiagnostics;
  std::vector<neuron::SemanticError> semanticDiagnostics;
  std::unordered_map<std::string, std::string> sourceByFile;
};

class ReplPipeline {
public:
  explicit ReplPipeline(ReplPipelineDependencies dependencies);

  ReplSubmitResult submit(ReplSession *session, const std::string &sourceText);
  static bool needsContinuation(const std::string &buffer);

private:
  ReplPipelineDependencies m_deps;
};

} // namespace neuron::cli
