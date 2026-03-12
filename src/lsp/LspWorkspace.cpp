#include "LspWorkspace.h"

#include "LspPath.h"

#include <algorithm>
#include <sstream>
#include <unordered_map>

namespace neuron::lsp::detail {

namespace {

void collectWorkspaceModuleFiles(
    const fs::path &dir, bool projectLocal,
    std::unordered_map<std::string, WorkspaceModuleIndexEntry> *index) {
  if (index == nullptr) {
    return;
  }

  std::error_code ec;
  if (!fs::exists(dir, ec) || !fs::is_directory(dir, ec)) {
    return;
  }

  for (fs::recursive_directory_iterator it(dir, ec), end; it != end;
       it.increment(ec)) {
    if (ec || !it->is_regular_file() || it->path().extension() != ".npp") {
      continue;
    }
    const std::string moduleName =
        normalizeModuleName(it->path().stem().string());
    auto existing = index->find(moduleName);
    if (existing == index->end() ||
        (projectLocal && !existing->second.projectLocal)) {
      (*index)[moduleName] = {normalizePath(it->path()), projectLocal};
    }
  }
}

std::unordered_map<std::string, WorkspaceModuleIndexEntry>
buildWorkspaceModuleIndex(const fs::path &root) {
  std::unordered_map<std::string, WorkspaceModuleIndexEntry> index;
  if (!root.empty()) {
    collectWorkspaceModuleFiles(root / "src", true, &index);
    collectWorkspaceModuleFiles(root / "modules", false, &index);
  }
  return index;
}

std::vector<std::string> extractImportedModules(const std::string &sourceText) {
  std::vector<std::string> modules;
  std::istringstream stream(sourceText);
  std::string line;
  while (std::getline(stream, line)) {
    std::string trimmed = trimCopy(line);
    if (trimmed.rfind("modulecpp ", 0) == 0) {
      continue;
    }
    if (trimmed.rfind("module ", 0) != 0) {
      continue;
    }
    trimmed = trimmed.substr(std::string("module ").size());
    const std::size_t semi = trimmed.find(';');
    if (semi == std::string::npos) {
      continue;
    }
    std::string moduleName = trimCopy(trimmed.substr(0, semi));
    if (!moduleName.empty()) {
      modules.push_back(moduleName);
    }
  }
  return modules;
}

} // namespace

std::unordered_set<std::string> collectAvailableModules(const fs::path &root,
                                                        const fs::path &path) {
  std::unordered_set<std::string> modules = {
      "system", "math",  "io",      "time",    "random", "logger",
      "tensor", "nn",    "dataset", "image",   "resource", "graphics"};

  const auto addStem = [&modules](std::string stem) {
    std::transform(stem.begin(), stem.end(), stem.begin(), [](unsigned char ch) {
      return static_cast<char>(std::tolower(ch));
    });
    modules.insert(std::move(stem));
  };

  if (!path.empty()) {
    addStem(path.stem().string());
  }

  const auto collectFromDir = [&](const fs::path &dir) {
    std::error_code ec;
    if (!fs::exists(dir, ec) || !fs::is_directory(dir, ec)) {
      return;
    }
    for (const auto &entry : fs::recursive_directory_iterator(dir, ec)) {
      if (ec || !entry.is_regular_file() || entry.path().extension() != ".npp") {
        continue;
      }
      addStem(entry.path().stem().string());
    }
  };

  if (!root.empty()) {
    collectFromDir(root / "src");
    collectFromDir(root / "modules");
  } else if (!path.empty()) {
    collectFromDir(path.parent_path());
  }

  return modules;
}

std::vector<ResolvedWorkspaceSource>
resolveWorkspaceSources(const fs::path &entryPath, const fs::path &workspaceRoot,
                        const WorkspaceFileReader &reader) {
  std::vector<ResolvedWorkspaceSource> resolved;
  if (reader == nullptr) {
    return resolved;
  }

  std::unordered_map<std::string, WorkspaceModuleIndexEntry> index =
      buildWorkspaceModuleIndex(workspaceRoot);
  const fs::path normalizedEntry = normalizePath(entryPath);
  if (index.empty()) {
    collectWorkspaceModuleFiles(normalizedEntry.parent_path(), true, &index);
  }
  index[normalizeModuleName(normalizedEntry.stem().string())] =
      {normalizedEntry, true};

  std::unordered_set<std::string> visited;
  std::unordered_set<std::string> active;

  std::function<void(const std::string &, const fs::path &)> visit;
  visit = [&](const std::string &moduleName, const fs::path &path) {
    const std::string normalizedName = normalizeModuleName(moduleName);
    if (visited.count(normalizedName) != 0u ||
        active.count(normalizedName) != 0u) {
      return;
    }

    const std::optional<std::string> text = reader(path);
    if (!text.has_value()) {
      return;
    }

    active.insert(normalizedName);
    for (const std::string &imported : extractImportedModules(*text)) {
      auto moduleIt = index.find(normalizeModuleName(imported));
      if (moduleIt == index.end()) {
        continue;
      }
      visit(imported, moduleIt->second.path);
    }
    active.erase(normalizedName);
    visited.insert(normalizedName);
    resolved.push_back({moduleName, normalizePath(path), *text});
  };

  visit(normalizedEntry.stem().string(), normalizedEntry);
  return resolved;
}

std::unique_ptr<ProgramNode>
mergeResolvedPrograms(std::vector<std::unique_ptr<ProgramNode>> *programs,
                      const std::string &entryModule) {
  auto merged =
      std::make_unique<ProgramNode>(SourceLocation{1, 1, entryModule});
  merged->moduleName = entryModule;

  std::unordered_set<std::string> seenModuleDecls;
  std::unordered_set<std::string> seenModuleCppDecls;
  if (programs == nullptr) {
    return merged;
  }

  for (auto &program : *programs) {
    if (program == nullptr) {
      continue;
    }
    for (auto &decl : program->declarations) {
      if (decl == nullptr) {
        continue;
      }
      if (decl->type == ASTNodeType::ModuleDecl) {
        const auto *moduleDecl =
            static_cast<const ModuleDeclNode *>(decl.get());
        if (!seenModuleDecls
                 .insert(normalizeModuleName(moduleDecl->moduleName))
                 .second) {
          continue;
        }
      }
      if (decl->type == ASTNodeType::ModuleCppDecl) {
        const auto *moduleDecl =
            static_cast<const ModuleCppDeclNode *>(decl.get());
        if (!seenModuleCppDecls
                 .insert(normalizeModuleName(moduleDecl->moduleName))
                 .second) {
          continue;
        }
      }
      merged->declarations.push_back(std::move(decl));
    }
  }

  return merged;
}

frontend::SemanticOptions makeSemanticOptions(const DocumentState &document,
                                              const fs::path &workspaceRoot) {
  frontend::SemanticOptions options;
  options.availableModules = collectAvailableModules(workspaceRoot, document.path);
  options.enforceModuleResolution = true;
  options.maxClassesPerFile = 1;
  options.requireMethodUppercaseStart = true;
  options.enforceStrictFileNaming = true;
  options.sourceFileStem = fileStem(document.path);
  options.maxLinesPerMethod = 50;
  options.maxLinesPerBlockStatement = 20;
  options.minMethodNameLength = 4;
  options.requireClassExplicitVisibility = true;
  options.requirePropertyExplicitVisibility = true;
  options.requireConstUppercase = true;
  options.maxNestingDepth = 3;
  options.requirePublicMethodDocs = true;
  options.agentHints = false;
  options.sourceText = document.text;
  return options;
}

frontend::SemanticOptions
makeWorkspaceSemanticOptions(const fs::path &entryPath,
                             const fs::path &workspaceRoot) {
  frontend::SemanticOptions options;
  options.availableModules = collectAvailableModules(workspaceRoot, entryPath);
  options.enforceModuleResolution = true;
  options.sourceFileStem = fileStem(entryPath);
  return options;
}

fs::path findWorkspaceRoot(const fs::path &path, const fs::path &fallback) {
  if (!fallback.empty()) {
    return fallback;
  }

  fs::path current = path.parent_path();
  while (!current.empty()) {
    std::error_code ec;
    if (fs::exists(current / "neuron.toml", ec) ||
        fs::exists(current / ".git", ec)) {
      return current;
    }
    const fs::path parent = current.parent_path();
    if (parent == current) {
      break;
    }
    current = parent;
  }
  return path.parent_path();
}

} // namespace neuron::lsp::detail
