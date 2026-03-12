#pragma once

#include "neuronc/cli/ModuleCppManifest.h"
#include "neuronc/cli/ProjectConfig.h"
#include "neuronc/sema/SemanticAnalyzer.h"

#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

namespace neuron {

struct LoadedModuleCppModule {
  std::string name;
  std::filesystem::path manifestPath;
  NconModuleCppConfig config;
  ModuleCppManifest manifest;
};

std::string normalizeModuleCppName(const std::string &name);

bool loadConfiguredModuleCppModules(
    const std::filesystem::path &projectRoot, const ProjectConfig &config,
    std::unordered_map<std::string, NativeModuleInfo> *outModules,
    std::vector<LoadedModuleCppModule> *outLoadedModules,
    std::vector<std::string> *outErrors);

bool loadProjectModuleCppModules(
    const std::filesystem::path &projectRoot, const ProjectConfig &config,
    std::unordered_map<std::string, NativeModuleInfo> *outModules,
    std::vector<LoadedModuleCppModule> *outLoadedModules,
    std::vector<std::string> *outErrors);

} // namespace neuron
