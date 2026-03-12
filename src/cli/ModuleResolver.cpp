#include "neuronc/cli/ModuleResolver.h"

#include "neuronc/cli/PackageManager.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <sstream>
#include <unordered_map>

namespace neuron {
namespace {
namespace fs = std::filesystem;

const char *kBuiltinModules[] = {"system",  "math",   "io",       "time",
                                 "random",  "logger", "tensor",   "nn",
                                 "dataset", "image",  "resource", "graphics",
                                 "filesystem"};

struct ModuleIndexEntry {
  fs::path path;
  bool projectLocal = false;
  ModuleProviderKind provider = ModuleProviderKind::Source;
};

struct ModuleImportInfo {
  std::string name;
  bool isExpand = false;
};

std::string trimCopy(const std::string &text) {
  std::size_t begin = 0;
  std::size_t end = text.size();
  while (begin < end &&
         std::isspace(static_cast<unsigned char>(text[begin])) != 0) {
    ++begin;
  }
  while (end > begin &&
         std::isspace(static_cast<unsigned char>(text[end - 1])) != 0) {
    --end;
  }
  return text.substr(begin, end - begin);
}

std::string toLowerCopy(const std::string &text) {
  std::string lowered = text;
  std::transform(lowered.begin(), lowered.end(), lowered.begin(),
                 [](unsigned char ch) {
                   return static_cast<char>(std::tolower(ch));
                 });
  return lowered;
}

fs::path detectProjectRoot(const fs::path &entryFile) {
  fs::path probe = entryFile;
  if (!fs::is_directory(probe)) {
    probe = probe.parent_path();
  }
  for (int depth = 0; depth < 8; ++depth) {
    if (fs::exists(probe / "neuron.toml")) {
      return probe;
    }
    const fs::path parent = probe.parent_path();
    if (parent == probe) {
      break;
    }
    probe = parent;
  }
  return entryFile.parent_path();
}

fs::path detectWorkspaceRoot(const fs::path &entryFile) {
  fs::path probe = entryFile;
  if (!fs::is_directory(probe)) {
    probe = probe.parent_path();
  }
  for (int depth = 0; depth < 8; ++depth) {
    if (fs::exists(probe / ".git") || fs::exists(probe / "README.md") ||
        fs::exists(probe / "TASK.md")) {
      return probe;
    }
    const fs::path parent = probe.parent_path();
    if (parent == probe) {
      break;
    }
    probe = parent;
  }
  return entryFile.parent_path();
}

fs::path builtinLibrariesRootFor(const fs::path &entryFile,
                                 const ModuleResolverOptions &options) {
  if (!options.builtinLibrariesRoot.empty()) {
    return options.builtinLibrariesRoot;
  }
  return detectWorkspaceRoot(entryFile) / "BuiltinLibraries";
}

fs::path builtinNativeLibrariesRootFor(const fs::path &entryFile,
                                       const ModuleResolverOptions &options) {
  if (!options.builtinNativeLibrariesRoot.empty()) {
    return options.builtinNativeLibrariesRoot;
  }
  return detectWorkspaceRoot(entryFile) / "BuiltinNativeLibraries";
}

void addBuiltins(std::unordered_set<std::string> *outModules) {
  if (outModules == nullptr) {
    return;
  }
  for (const char *builtin : kBuiltinModules) {
    outModules->insert(toLowerCopy(builtin));
  }
}

void recordModuleIndexEntry(const std::string &moduleName,
                            const ModuleIndexEntry &entry,
                            std::unordered_map<std::string, ModuleIndexEntry> *outIndex,
                            std::unordered_set<std::string> *outAvailableModules,
                            std::vector<std::string> *outErrors) {
  if (outIndex == nullptr || outAvailableModules == nullptr) {
    return;
  }

  const std::string normalized = toLowerCopy(moduleName);
  auto existing = outIndex->find(normalized);
  if (existing == outIndex->end()) {
    (*outIndex)[normalized] = entry;
    outAvailableModules->insert(normalized);
    return;
  }

  if (existing->second.provider == ModuleProviderKind::Source &&
      entry.provider == ModuleProviderKind::BuiltinNative) {
    return;
  }
  if (existing->second.provider == ModuleProviderKind::BuiltinNative &&
      entry.provider == ModuleProviderKind::Source) {
    existing->second = entry;
    outAvailableModules->insert(normalized);
    return;
  }
  if (existing->second.path != entry.path && outErrors != nullptr) {
    outErrors->push_back("module provider conflict detected for '" + moduleName +
                         "'");
  }
}

void collectBuiltinNativeLibraryContracts(
    const fs::path &builtinRoot,
    std::unordered_map<std::string, ModuleIndexEntry> *outIndex,
    std::unordered_set<std::string> *outAvailableModules,
    std::vector<std::string> *outErrors) {
  if (outIndex == nullptr || outAvailableModules == nullptr) {
    return;
  }
  std::error_code ec;
  if (!fs::exists(builtinRoot, ec) || !fs::is_directory(builtinRoot, ec)) {
    return;
  }

  for (const auto &entry : fs::directory_iterator(builtinRoot, ec)) {
    if (ec || !entry.is_directory()) {
      continue;
    }
    const std::string moduleName = entry.path().filename().string();
    const fs::path contractPath = entry.path() / (moduleName + ".npp");
    const fs::path manifestPath = entry.path() / "native.toml";
    if (!fs::exists(contractPath, ec) || !fs::is_regular_file(contractPath, ec)) {
      if (outErrors != nullptr) {
        outErrors->push_back("builtin native contract not found for '" +
                             moduleName + "': expected " + contractPath.string());
      }
      continue;
    }
    if (!fs::exists(manifestPath, ec) || !fs::is_regular_file(manifestPath, ec)) {
      if (outErrors != nullptr) {
        outErrors->push_back("builtin native manifest not found for '" +
                             moduleName + "': expected " + manifestPath.string());
      }
      continue;
    }
    recordModuleIndexEntry(moduleName,
                           {contractPath, false, ModuleProviderKind::BuiltinNative},
                           outIndex, outAvailableModules, outErrors);
  }
}

void collectModuleFiles(const fs::path &root, const std::string &sourceDir,
                        bool projectLocal,
                        std::unordered_map<std::string, ModuleIndexEntry> *outIndex,
                        std::unordered_set<std::string> *outAvailableModules,
                        std::vector<std::string> *outErrors) {
  if (outIndex == nullptr) {
    return;
  }
  const fs::path sourceRoot = (root / sourceDir).lexically_normal();
  std::error_code ec;
  if (!fs::exists(sourceRoot, ec) || !fs::is_directory(sourceRoot, ec)) {
    return;
  }

  for (fs::recursive_directory_iterator it(sourceRoot, ec), end; it != end;
       it.increment(ec)) {
    if (ec || !it->is_regular_file() || it->path().extension() != ".npp") {
      continue;
    }
    recordModuleIndexEntry(it->path().stem().string(),
                           {it->path(), projectLocal, ModuleProviderKind::Source},
                           outIndex, outAvailableModules, outErrors);
  }
}

void collectBuiltinLibraryFacadeModules(
    const fs::path &builtinRoot,
    std::unordered_map<std::string, ModuleIndexEntry> *outIndex,
    std::unordered_set<std::string> *outAvailableModules,
    std::vector<std::string> *outErrors) {
  if (outIndex == nullptr || outAvailableModules == nullptr) {
    return;
  }
  std::error_code ec;
  if (!fs::exists(builtinRoot, ec) || !fs::is_directory(builtinRoot, ec)) {
    return;
  }

  for (const auto &entry : fs::directory_iterator(builtinRoot, ec)) {
    if (ec || !entry.is_directory()) {
      continue;
    }
    const std::string moduleName = entry.path().filename().string();
    const fs::path facadePath = entry.path() / (moduleName + ".npp");
    if (!fs::exists(facadePath, ec) || !fs::is_regular_file(facadePath, ec)) {
      if (outErrors != nullptr) {
        outErrors->push_back("builtin library facade not found for '" +
                             moduleName + "': expected " +
                             facadePath.string());
      }
      continue;
    }
    recordModuleIndexEntry(moduleName,
                           {facadePath, false, ModuleProviderKind::BuiltinSource},
                           outIndex, outAvailableModules, outErrors);
  }
}

void buildModuleIndex(const fs::path &projectRoot,
                      const std::optional<ProjectConfig> &config,
                      std::unordered_map<std::string, ModuleIndexEntry> *outIndex,
                      std::unordered_set<std::string> *outAvailableModules,
                      std::vector<std::string> *outErrors,
                      const fs::path &builtinLibrariesRoot,
                      const fs::path &builtinNativeLibrariesRoot) {
  if (outIndex == nullptr || outAvailableModules == nullptr) {
    return;
  }
  outIndex->clear();
  outAvailableModules->clear();
  addBuiltins(outAvailableModules);
  collectBuiltinLibraryFacadeModules(builtinLibrariesRoot, outIndex,
                                     outAvailableModules, outErrors);
  collectBuiltinNativeLibraryContracts(builtinNativeLibrariesRoot, outIndex,
                                       outAvailableModules, outErrors);

  const std::string projectSourceDir =
      config.has_value() && !config->package.sourceDir.empty()
          ? config->package.sourceDir
          : std::string("src");
  collectModuleFiles(projectRoot, projectSourceDir, true, outIndex,
                     outAvailableModules, outErrors);
  if (config.has_value() && !config->mainFile.empty()) {
    collectModuleFiles(projectRoot, fs::path(config->mainFile).parent_path().string(),
                       true, outIndex, outAvailableModules, outErrors);
  }

  const fs::path modulesRoot = projectRoot / "modules";
  std::error_code ec;
  if (fs::exists(modulesRoot, ec) && fs::is_directory(modulesRoot, ec)) {
    for (const auto &entry : fs::directory_iterator(modulesRoot, ec)) {
      if (ec || !entry.is_directory()) {
        continue;
      }
      ProjectConfig packageConfig;
      std::vector<std::string> configErrors;
      if (!PackageManager::loadProjectConfig(entry.path().string(), &packageConfig,
                                             &configErrors)) {
        continue;
      }
      collectModuleFiles(entry.path(), packageConfig.package.sourceDir, false,
                         outIndex, outAvailableModules, outErrors);
    }
  }

  for (const auto &entry : *outIndex) {
    outAvailableModules->insert(entry.first);
  }
}

std::vector<ModuleImportInfo> extractModuleImports(const std::string &sourceText) {
  std::vector<ModuleImportInfo> modules;
  std::istringstream stream(sourceText);
  std::string line;
  while (std::getline(stream, line)) {
    std::string trimmed = trimCopy(line);
    if (trimmed.rfind("modulecpp ", 0) == 0) {
      continue;
    }

    bool isExpand = false;
    if (trimmed.rfind("expand module ", 0) == 0) {
      trimmed = trimmed.substr(std::string("expand module ").size());
      isExpand = true;
    } else if (trimmed.rfind("module ", 0) == 0) {
      trimmed = trimmed.substr(std::string("module ").size());
    } else {
      continue;
    }

    const std::size_t semi = trimmed.find(';');
    if (semi == std::string::npos) {
      continue;
    }
    std::string moduleName = trimCopy(trimmed.substr(0, semi));
    if (!moduleName.empty()) {
      modules.push_back({moduleName, isExpand});
    }
  }
  return modules;
}

} // namespace

ModuleResolverResult
ModuleResolver::resolve(const std::filesystem::path &entryFile,
                        const std::optional<ProjectConfig> &config,
                        const SourceReader &reader,
                        const ModuleResolverOptions &options) {
  ModuleResolverResult result;
  result.projectRoot =
      config.has_value() ? detectProjectRoot(entryFile) : detectProjectRoot(entryFile);
  const fs::path builtinLibrariesRoot =
      options.autoIncludeBuiltinLibraries
          ? builtinLibrariesRootFor(entryFile, options)
          : fs::path();
  const fs::path builtinNativeLibrariesRoot =
      options.autoIncludeBuiltinNativeLibraries
          ? builtinNativeLibrariesRootFor(entryFile, options)
          : fs::path();

  std::unordered_map<std::string, ModuleIndexEntry> index;
  buildModuleIndex(result.projectRoot, config, &index, &result.availableModules,
                   &result.errors, builtinLibrariesRoot,
                   builtinNativeLibrariesRoot);
  for (const auto &entry : index) {
    result.moduleProviders[entry.first] = entry.second.provider;
  }

  std::unordered_set<std::string> visited;
  std::unordered_set<std::string> active;
  std::unordered_set<std::string> attemptedAutoAdd;

  std::function<void(const std::string &, const fs::path &)> resolveFile;
  resolveFile = [&](const std::string &moduleName, const fs::path &path) {
    const std::string normalized = toLowerCopy(moduleName);
    if (visited.count(normalized) != 0u) {
      return;
    }
    if (active.count(normalized) != 0u) {
      result.errors.push_back("Circular expand module dependency detected for '" +
                              moduleName + "'");
      return;
    }
    active.insert(normalized);

    std::string readError;
    std::string sourceText = reader(path, &readError);
    if (sourceText.empty() && !readError.empty()) {
      result.errors.push_back(readError);
      active.erase(normalized);
      return;
    }

    for (const auto &imported : extractModuleImports(sourceText)) {
      const std::string importedNormalized = toLowerCopy(imported.name);
      if (result.availableModules.count(importedNormalized) == 0u &&
          options.autoAddMissingPackages &&
          attemptedAutoAdd.insert(importedNormalized).second) {
        std::string message;
        if (PackageManager::autoAddMissingModule(result.projectRoot.string(),
                                                 imported.name, &message)) {
          buildModuleIndex(result.projectRoot, config, &index,
                           &result.availableModules, &result.errors,
                           builtinLibrariesRoot, builtinNativeLibrariesRoot);
          result.moduleProviders.clear();
          for (const auto &entry : index) {
            result.moduleProviders[entry.first] = entry.second.provider;
          }
        }
      }

      if (result.availableModules.count(importedNormalized) == 0u) {
        result.errors.push_back("Unknown module: " + imported.name);
        continue;
      }
      if (!imported.isExpand) {
        continue;
      }
      auto importedIt = index.find(importedNormalized);
      if (importedIt == index.end()) {
        continue;
      }
      resolveFile(imported.name, importedIt->second.path);
    }

    result.orderedSources.push_back({moduleName, path, sourceText});
    visited.insert(normalized);
    active.erase(normalized);
  };

  resolveFile(entryFile.stem().string(), entryFile);
  return result;
}

} // namespace neuron
