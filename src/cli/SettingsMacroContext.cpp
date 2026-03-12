#include "SettingsMacroInternal.h"

#include <algorithm>
#include <utility>

namespace neuron::cli {

namespace {

const char *kBuiltinSectionNames[] = {
    "System", "Math",   "IO",      "Time",   "Random", "Logger",
    "Tensor", "NN",     "Dataset", "Image",  "Resource", "Graphics",
};

} // namespace

bool SettingsMacroProcessor::initialize(std::vector<std::string> *outErrors) {
  if (outErrors != nullptr) {
    outErrors->clear();
  }

  m_impl->ctx.entryRoot = detectEntryProjectRoot(m_entrySourcePath);
  m_impl->ctx.roots = discoverProjectRoots(m_impl->ctx.entryRoot);

  if (const auto builtinPath = resolveBuiltinSettingsPath(m_toolRoot);
      builtinPath.has_value()) {
    std::string builtinText;
    if (!readTextFile(*builtinPath, &builtinText)) {
      outErrors->push_back(makeConfigError(*builtinPath, 1, 1,
                                           "Failed to read builtin settings."));
      return false;
    }
    std::unordered_map<std::string, std::unordered_map<std::string, ProjectOverride>>
        unusedOverrides;
    SettingsFileParser parser(*builtinPath, builtinText, false);
    if (!parser.parse(&m_impl->ctx.builtinDefaults, &unusedOverrides, outErrors,
                      fs::path())) {
      return false;
    }
  }

  std::unordered_map<std::string, std::string> sectionOwners;
  for (const auto *builtin : kBuiltinSectionNames) {
    sectionOwners.emplace(lowerAscii(builtin), "__builtin__");
  }

  for (const auto &root : m_impl->ctx.roots) {
    ProjectSettingsData project;
    project.root = root;
    project.ownedSections = collectOwnedSectionsForRoot(root);

    const fs::path defaultsPath = root / ".modulesettings";
    if (fs::exists(defaultsPath)) {
      std::string text;
      if (!readTextFile(defaultsPath, &text)) {
        outErrors->push_back(
            makeConfigError(defaultsPath, 1, 1, "Failed to read .modulesettings."));
        return false;
      }
      std::unordered_map<std::string, std::unordered_map<std::string, ProjectOverride>>
          unusedOverrides;
      SettingsFileParser parser(defaultsPath, text, false);
      if (!parser.parse(&project.defaults, &unusedOverrides, outErrors, root)) {
        return false;
      }
    }

    for (const auto &sectionEntry : project.defaults) {
      if (project.ownedSections.find(sectionEntry.first) == project.ownedSections.end()) {
        const auto &first = sectionEntry.second.begin()->second;
        outErrors->push_back(makeConfigError(
            first.originFile, first.originLine, first.originColumn,
            "Section '" + first.section +
                "' is not owned by this project; .modulesettings may only "
                "define local module defaults."));
        return false;
      }
      const std::string rootName = pathKey(root);
      auto ownerIt = sectionOwners.find(sectionEntry.first);
      if (ownerIt != sectionOwners.end() && ownerIt->second != rootName) {
        const auto &first = sectionEntry.second.begin()->second;
        outErrors->push_back(makeConfigError(
            first.originFile, first.originLine, first.originColumn,
            "Section '" + first.section +
                "' is already defined by another module or builtin."));
        return false;
      }
      sectionOwners[sectionEntry.first] = rootName;
    }

    const fs::path overridesPath = root / ".projectsettings";
    if (fs::exists(overridesPath)) {
      std::string text;
      if (!readTextFile(overridesPath, &text)) {
        outErrors->push_back(makeConfigError(overridesPath, 1, 1,
                                             "Failed to read .projectsettings."));
        return false;
      }
      std::unordered_map<std::string,
                         std::unordered_map<std::string, MacroDefinition>>
          unusedDefaults;
      SettingsFileParser parser(overridesPath, text, true);
      if (!parser.parse(&unusedDefaults, &project.overrides, outErrors, root)) {
        return false;
      }
    }

    m_impl->ctx.projects[pathKey(root)] = std::move(project);
  }

  for (const auto &root : m_impl->ctx.roots) {
    const std::string rootKey = pathKey(root);
    for (const auto &candidate : m_impl->ctx.roots) {
      if (pathStartsWith(candidate, root)) {
        m_impl->ctx.descendantsByRoot[rootKey].push_back(pathKey(candidate));
      }
    }
    std::sort(m_impl->ctx.descendantsByRoot[rootKey].begin(),
              m_impl->ctx.descendantsByRoot[rootKey].end(),
              [](const std::string &lhs, const std::string &rhs) {
                return lhs.size() < rhs.size();
              });
    m_impl->ctx.chainByRoot[rootKey] = sourceRootChain(root, m_impl->ctx.roots);
    if (!validateProjectOverrides(m_impl->ctx, rootKey, outErrors)) {
      return false;
    }
    m_impl->ctx.effectiveByOwnerRoot[rootKey] =
        buildEffectiveSetForRoot(m_impl->ctx, rootKey);
  }

  return true;
}

} // namespace neuron::cli
