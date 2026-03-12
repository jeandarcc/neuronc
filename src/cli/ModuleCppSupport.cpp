#include "neuronc/cli/ModuleCppSupport.h"

#include "neuronc/cli/PackageManager.h"

#include <algorithm>
#include <cctype>
#include <vector>

namespace neuron {

namespace {

bool appendConfiguredModuleCppModules(
    const std::filesystem::path &projectRoot, const ProjectConfig &config,
    std::unordered_map<std::string, NativeModuleInfo> *outModules,
    std::vector<LoadedModuleCppModule> *outLoadedModules,
    std::vector<std::string> *outErrors) {
  if (outModules == nullptr) {
    if (outErrors != nullptr) {
      outErrors->push_back("internal error: null modulecpp output");
    }
    return false;
  }

  if (!config.ncon.native.enabled) {
    return true;
  }

  bool ok = true;
  std::vector<std::string> moduleNames;
  moduleNames.reserve(config.ncon.native.modules.size());
  for (const auto &entry : config.ncon.native.modules) {
    moduleNames.push_back(entry.first);
  }
  std::sort(moduleNames.begin(), moduleNames.end());

  for (const auto &moduleName : moduleNames) {
    const auto &entry = *config.ncon.native.modules.find(moduleName);
    const std::string normalizedName = normalizeModuleCppName(entry.first);
    const NconModuleCppConfig &moduleConfig = entry.second;
    if (moduleConfig.manifestPath.empty()) {
      ok = false;
      if (outErrors != nullptr) {
        outErrors->push_back("modulecpp '" + entry.first +
                             "' is missing a manifest path");
      }
      continue;
    }
    if (outModules->find(normalizedName) != outModules->end()) {
      ok = false;
      if (outErrors != nullptr) {
        outErrors->push_back("duplicate modulecpp module detected: '" +
                             entry.first + "'");
      }
      continue;
    }

    LoadedModuleCppModule loaded;
    loaded.name = entry.first;
    loaded.config = moduleConfig;
    loaded.manifestPath = (projectRoot / moduleConfig.manifestPath).lexically_normal();

    ModuleCppManifestParser parser;
    if (!parser.parseFile(loaded.manifestPath.string(), &loaded.manifest)) {
      ok = false;
      if (outErrors != nullptr) {
        for (const auto &error : parser.errors()) {
          outErrors->push_back(error);
        }
      }
      continue;
    }

    if (!loaded.manifest.name.empty() &&
        normalizeModuleCppName(loaded.manifest.name) != normalizedName) {
      ok = false;
      if (outErrors != nullptr) {
        outErrors->push_back("modulecpp manifest name mismatch for '" +
                             entry.first + "': expected '" + entry.first +
                             "', found '" + loaded.manifest.name + "'");
      }
      continue;
    }

    NativeModuleInfo info;
    info.name = entry.first;
    for (const auto &exportEntry : loaded.manifest.exports) {
      NativeModuleExportSignature signature;
      signature.symbolName = exportEntry.second.symbol;
      signature.parameterTypeNames = exportEntry.second.parameterTypes;
      signature.returnTypeName = exportEntry.second.returnType;
      info.exports[exportEntry.first] = std::move(signature);
    }

    (*outModules)[normalizedName] = std::move(info);
    if (outLoadedModules != nullptr) {
      outLoadedModules->push_back(std::move(loaded));
    }
  }

  return ok;
}

} // namespace

std::string normalizeModuleCppName(const std::string &name) {
  std::string normalized = name;
  std::transform(normalized.begin(), normalized.end(), normalized.begin(),
                 [](unsigned char ch) {
                   return static_cast<char>(std::tolower(ch));
                 });
  return normalized;
}

bool loadConfiguredModuleCppModules(
    const std::filesystem::path &projectRoot, const ProjectConfig &config,
    std::unordered_map<std::string, NativeModuleInfo> *outModules,
    std::vector<LoadedModuleCppModule> *outLoadedModules,
    std::vector<std::string> *outErrors) {
  if (outModules == nullptr) {
    if (outErrors != nullptr) {
      outErrors->push_back("internal error: null modulecpp output");
    }
    return false;
  }

  outModules->clear();
  if (outLoadedModules != nullptr) {
    outLoadedModules->clear();
  }
  return appendConfiguredModuleCppModules(projectRoot, config, outModules,
                                          outLoadedModules, outErrors);
}

bool loadProjectModuleCppModules(
    const std::filesystem::path &projectRoot, const ProjectConfig &config,
    std::unordered_map<std::string, NativeModuleInfo> *outModules,
    std::vector<LoadedModuleCppModule> *outLoadedModules,
    std::vector<std::string> *outErrors) {
  if (outModules == nullptr) {
    if (outErrors != nullptr) {
      outErrors->push_back("internal error: null modulecpp output");
    }
    return false;
  }

  outModules->clear();
  if (outLoadedModules != nullptr) {
    outLoadedModules->clear();
  }

  bool ok = appendConfiguredModuleCppModules(projectRoot, config, outModules,
                                             outLoadedModules, outErrors);

  const std::filesystem::path modulesRoot = projectRoot / "modules";
  std::error_code ec;
  if (!std::filesystem::exists(modulesRoot, ec) ||
      !std::filesystem::is_directory(modulesRoot, ec)) {
    return ok;
  }

  for (const auto &entry : std::filesystem::directory_iterator(modulesRoot, ec)) {
    if (ec || !entry.is_directory()) {
      continue;
    }

    ProjectConfig packageConfig;
    std::vector<std::string> configErrors;
    if (!PackageManager::loadProjectConfig(entry.path().string(), &packageConfig,
                                           &configErrors)) {
      continue;
    }
    if (!appendConfiguredModuleCppModules(entry.path(), packageConfig, outModules,
                                          outLoadedModules, outErrors)) {
      ok = false;
    }
  }

  return ok;
}

} // namespace neuron
