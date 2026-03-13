// ProjectHelpers.cpp â€” Proje yapÄ±landÄ±rma yardÄ±mcÄ±larÄ± implementasyonu.
// Bkz. ProjectHelpers.h
#include "ProjectHelpers.h"
#include "AppGlobals.h"
#include "SettingsLoader.h"

#include "neuronc/cli/ModuleCppSupport.h"
#include "neuronc/cli/ProjectConfig.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>

// â”€â”€ Internal helpers â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€

static std::string normalizeName(std::string text) {
  std::transform(text.begin(), text.end(), text.begin(), [](unsigned char c) {
    return static_cast<char>(std::tolower(c));
  });
  return text;
}

static void collectModulesFromDir(const fs::path &dir,
                                  std::unordered_set<std::string> *out) {
  if (out == nullptr || !fs::exists(dir) || !fs::is_directory(dir)) {
    return;
  }
  for (const auto &entry : fs::recursive_directory_iterator(dir)) {
    if (!entry.is_regular_file() || entry.path().extension() != ".nr") {
      continue;
    }
    out->insert(normalizeName(entry.path().stem().string()));
  }
}

// â”€â”€ Source file reading â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€

std::string readFile(const std::string &path, const NeuronSettings &settings) {
  if (!validateScriptPolicy(fs::path(path), settings)) {
    return "";
  }

  std::ifstream file(path);
  if (!file.is_open()) {
    std::cerr << "Error: Cannot open file '" << path << "'" << std::endl;
    return "";
  }

  std::ostringstream ss;
  std::string line;
  std::size_t lineCount = 0;
  bool firstLine = true;

  while (std::getline(file, line)) {
    lineCount++;
    if (settings.maxLinesPerFile > 0 &&
        lineCount > static_cast<std::size_t>(settings.maxLinesPerFile)) {
      std::cerr << "Error: File '" << path
                << "' exceeds maximum allowed length ("
                << settings.maxLinesPerFile
                << " lines). Configure max_lines_per_file in .neuronsettings."
                << std::endl;
      return "";
    }
    if (!firstLine) {
      ss << '\n';
    }
    ss << line;
    firstLine = false;
  }

  if (file.bad()) {
    std::cerr << "Error: Failed while reading file '" << path << "'"
              << std::endl;
    return "";
  }

  return ss.str();
}

// â”€â”€ Project configuration â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€

bool loadProjectConfigFromCwd(neuron::ProjectConfig *outConfig,
                              std::vector<std::string> *outErrors) {
  if (outConfig == nullptr) {
    return false;
  }
  if (!fs::exists("neuron.toml")) {
    if (outErrors != nullptr) {
      outErrors->push_back("Error: No neuron.toml found in current directory.");
    }
    return false;
  }
  neuron::ProjectConfigParser parser;
  if (!parser.parseFile("neuron.toml", outConfig)) {
    if (outErrors != nullptr) {
      outErrors->insert(outErrors->end(), parser.errors().begin(),
                        parser.errors().end());
    }
    return false;
  }
  return true;
}

std::optional<neuron::ProjectConfig> tryLoadProjectConfigFromCwd() {
  if (!fs::exists("neuron.toml")) {
    return std::nullopt;
  }
  neuron::ProjectConfig cfg;
  neuron::ProjectConfigParser parser;
  if (!parser.parseFile("neuron.toml", &cfg)) {
    return std::nullopt;
  }
  return cfg;
}

// â”€â”€ Module collection â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€

std::unordered_set<std::string>
collectImportedModuleCppModules(const neuron::ProgramNode *program) {
  std::unordered_set<std::string> modules;
  if (program == nullptr) {
    return modules;
  }
  for (const auto &decl : program->declarations) {
    if (decl->type != neuron::ASTNodeType::ModuleCppDecl) {
      continue;
    }
    auto *module = static_cast<neuron::ModuleCppDeclNode *>(decl.get());
    modules.insert(neuron::normalizeModuleCppName(module->moduleName));
  }
  return modules;
}

std::unordered_set<std::string>
collectAvailableModules(const fs::path &sourceFile,
                        const std::optional<neuron::ProjectConfig> &config) {
  std::unordered_set<std::string> modules;

  const char *builtinModules[] = {"system",  "math",   "io",       "time",
                                  "random",  "logger", "tensor",   "nn",
                                  "dataset", "image",  "resource", "graphics"};
  for (const char *name : builtinModules) {
    modules.insert(name);
  }

  if (!sourceFile.empty()) {
    modules.insert(normalizeName(sourceFile.stem().string()));
  }

  fs::path projectRoot = fs::current_path();
  if (!sourceFile.empty()) {
    fs::path candidateRoot = sourceFile.parent_path().parent_path();
    if (fs::exists(candidateRoot / "neuron.toml")) {
      projectRoot = candidateRoot;
    } else if (fs::exists(fs::current_path() / "neuron.toml")) {
      projectRoot = fs::current_path();
    } else {
      projectRoot = sourceFile.parent_path();
    }
  }

  collectModulesFromDir(projectRoot / "modules", &modules);
  collectModulesFromDir(projectRoot / "src", &modules);

  if (config.has_value()) {
    for (const auto &dep : config->dependencies) {
      modules.insert(normalizeName(dep.first));
    }
  }

  return modules;
}

// â”€â”€ SemanticAnalyzer configuration â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€

void configureSemanticAnalyzerModules(
    neuron::SemanticAnalyzer *sema, const fs::path &sourceFile,
    const std::optional<neuron::ProjectConfig> &config,
    const NeuronSettings &settings, const std::string &sourceText,
    std::vector<std::string> *outConfigErrors) {
  if (sema == nullptr) {
    return;
  }
  sema->setAvailableModules(collectAvailableModules(sourceFile, config), true);
  sema->setMaxClassesPerFile(settings.maxClassesPerFile);
  sema->setRequireMethodUppercaseStart(settings.requireMethodUppercaseStart);
  sema->setStrictFileNamingRules(settings.enforceStrictFileNaming,
                                 sourceFile.stem().string());
  sema->setMaxLinesPerMethod(settings.maxLinesPerMethod);
  sema->setMaxLinesPerBlockStatement(settings.maxLinesPerBlockStatement);
  sema->setMinMethodNameLength(settings.minMethodNameLength);
  sema->setRequireClassExplicitVisibility(
      settings.requireClassExplicitVisibility);
  sema->setRequirePropertyExplicitVisibility(
      settings.requirePropertyExplicitVisibility);
  sema->setRequireConstUppercase(settings.requireConstUppercase);
  sema->setMaxNestingDepth(settings.maxNestingDepth);
  sema->setRequirePublicMethodDocs(settings.requirePublicMethodDocs);
  sema->setSourceText(sourceText);
  sema->setAgentHints(settings.agentHints);

  if (!config.has_value()) {
    return;
  }

  std::unordered_map<std::string, neuron::NativeModuleInfo> moduleCppModules;
  std::vector<neuron::LoadedModuleCppModule> loadedModules;
  std::vector<std::string> moduleErrors;
  if (!neuron::loadConfiguredModuleCppModules(
          sourceFile.parent_path().parent_path(), *config, &moduleCppModules,
          &loadedModules, &moduleErrors)) {
    if (outConfigErrors != nullptr) {
      outConfigErrors->insert(outConfigErrors->end(), moduleErrors.begin(),
                              moduleErrors.end());
    }
  }
  sema->setModuleCppModules(moduleCppModules);
}

// â”€â”€ NIR helpers â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€

std::size_t countNIRInstructions(const neuron::nir::Module *module) {
  if (module == nullptr) {
    return 0;
  }
  std::size_t count = 0;
  for (const auto &fn : module->getFunctions()) {
    for (const auto &block : fn->getBlocks()) {
      count += block->getInstructions().size();
    }
  }
  return count;
}
