#pragma once

#include "neuronc/cli/ProjectConfig.h"

#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace neuron {

struct ResolvedModuleSource {
  std::string moduleName;
  std::filesystem::path path;
  std::string sourceText;
};

enum class ModuleProviderKind {
  Source,
  BuiltinSource,
  BuiltinNative,
  ExternalNative,
};

struct ModuleResolverOptions {
  bool autoAddMissingPackages = false;
  bool autoIncludeBuiltinLibraries = true;
  bool autoIncludeBuiltinNativeLibraries = true;
  std::filesystem::path builtinLibrariesRoot;
  std::filesystem::path builtinNativeLibrariesRoot;
};

struct ModuleResolverResult {
  std::filesystem::path projectRoot;
  std::vector<ResolvedModuleSource> orderedSources;
  std::unordered_set<std::string> availableModules;
  std::unordered_map<std::string, ModuleProviderKind> moduleProviders;
  std::vector<std::string> errors;
};

class ModuleResolver {
public:
  using SourceReader =
      std::function<std::string(const std::filesystem::path &, std::string *)>;

  static ModuleResolverResult
  resolve(const std::filesystem::path &entryFile,
          const std::optional<ProjectConfig> &config, const SourceReader &reader,
          const ModuleResolverOptions &options = {});
};

} // namespace neuron
