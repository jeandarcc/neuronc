#include "SettingsMacroInternal.h"

#include <algorithm>
#include <limits>
#include <unordered_map>
#include <utility>

namespace neuron::cli {

namespace {

using MacroMap =
    std::unordered_map<std::string, std::unordered_map<std::string, MacroDefinition>>;

MacroMap buildVisibleDefaults(const SettingsMacroContext &ctx,
                              const std::string &ownerRootKey) {
  MacroMap visible = ctx.builtinDefaults;
  const auto descendantsIt = ctx.descendantsByRoot.find(ownerRootKey);
  if (descendantsIt == ctx.descendantsByRoot.end()) {
    return visible;
  }

  for (const std::string &descendantKey : descendantsIt->second) {
    const auto projectIt = ctx.projects.find(descendantKey);
    if (projectIt == ctx.projects.end()) {
      continue;
    }
    for (const auto &sectionEntry : projectIt->second.defaults) {
      for (const auto &macroEntry : sectionEntry.second) {
        visible[sectionEntry.first].insert(macroEntry);
      }
    }
  }

  return visible;
}

} // namespace

bool validateProjectOverrides(const SettingsMacroContext &ctx,
                              const std::string &projectRootKey,
                              std::vector<std::string> *outErrors) {
  const auto projectIt = ctx.projects.find(projectRootKey);
  if (projectIt == ctx.projects.end()) {
    return true;
  }

  const MacroMap visible = buildVisibleDefaults(ctx, projectRootKey);
  for (const auto &sectionEntry : projectIt->second.overrides) {
    const auto visibleSectionIt = visible.find(sectionEntry.first);
    if (visibleSectionIt == visible.end()) {
      const auto &first = sectionEntry.second.begin()->second;
      outErrors->push_back(makeConfigError(
          first.originFile, first.originLine, first.originColumn,
          "Unknown override section '" + first.section + "'."));
      return false;
    }
    for (const auto &overrideEntry : sectionEntry.second) {
      const auto visibleKeyIt = visibleSectionIt->second.find(overrideEntry.first);
      if (visibleKeyIt == visibleSectionIt->second.end()) {
        const auto &entry = overrideEntry.second;
        outErrors->push_back(makeConfigError(
            entry.originFile, entry.originLine, entry.originColumn,
            "Unknown override key '" + entry.section + "." + entry.name + "'."));
        return false;
      }
      if (visibleKeyIt->second.kind != overrideEntry.second.kind) {
        const auto &entry = overrideEntry.second;
        outErrors->push_back(makeConfigError(
            entry.originFile, entry.originLine, entry.originColumn,
            "Override kind mismatch for '" + entry.section + "." + entry.name +
                "'."));
        return false;
      }
    }
  }

  return true;
}

EffectiveMacroSet buildEffectiveSetForRoot(const SettingsMacroContext &ctx,
                                           const std::string &ownerRootKey) {
  const MacroMap visible = buildVisibleDefaults(ctx, ownerRootKey);
  EffectiveMacroSet effective;
  std::unordered_map<std::string, EffectiveMacroEntry> selectedOverrides;
  std::unordered_map<std::string, std::size_t> selectedPrecedence;
  std::unordered_map<std::string, std::vector<EffectiveMacroEntry>> byName;
  const auto chainIt = ctx.chainByRoot.find(ownerRootKey);

  if (chainIt != ctx.chainByRoot.end()) {
    for (std::size_t precedence = 0; precedence < chainIt->second.size();
         ++precedence) {
      const auto projectIt = ctx.projects.find(chainIt->second[precedence]);
      if (projectIt == ctx.projects.end()) {
        continue;
      }
      for (const auto &sectionEntry : projectIt->second.overrides) {
        const auto visibleSectionIt = visible.find(sectionEntry.first);
        if (visibleSectionIt == visible.end()) {
          continue;
        }
        for (const auto &overrideEntry : sectionEntry.second) {
          if (visibleSectionIt->second.find(overrideEntry.first) ==
              visibleSectionIt->second.end()) {
            continue;
          }

          const std::string qualified =
              qualifiedMacroKey(sectionEntry.first, overrideEntry.first);
          EffectiveMacroEntry candidate;
          candidate.section = overrideEntry.second.section;
          candidate.normalizedSection = overrideEntry.second.normalizedSection;
          candidate.name = overrideEntry.second.name;
          candidate.kind = overrideEntry.second.kind;
          candidate.rawSnippet = overrideEntry.second.rawSnippet;
          candidate.importance = overrideEntry.second.importance;
          candidate.originFile = overrideEntry.second.originFile;
          candidate.originLine = overrideEntry.second.originLine;
          candidate.originColumn = overrideEntry.second.originColumn;

          const auto existing = selectedOverrides.find(qualified);
          if (existing == selectedOverrides.end() ||
              candidate.importance > existing->second.importance ||
              (candidate.importance == existing->second.importance &&
               precedence >= selectedPrecedence[qualified])) {
            selectedPrecedence[qualified] = precedence;
            selectedOverrides[qualified] = std::move(candidate);
          }
        }
      }
    }
  }

  for (const auto &sectionEntry : visible) {
    for (const auto &macroEntry : sectionEntry.second) {
      const std::string qualified =
          qualifiedMacroKey(sectionEntry.first, macroEntry.first);
      EffectiveMacroEntry entry;
      entry.section = macroEntry.second.section;
      entry.normalizedSection = macroEntry.second.normalizedSection;
      entry.name = macroEntry.second.name;
      entry.kind = macroEntry.second.kind;
      entry.rawSnippet = macroEntry.second.rawSnippet;
      entry.importance = std::numeric_limits<int>::min();
      entry.originFile = macroEntry.second.originFile;
      entry.originLine = macroEntry.second.originLine;
      entry.originColumn = macroEntry.second.originColumn;
      if (const auto overrideIt = selectedOverrides.find(qualified);
          overrideIt != selectedOverrides.end()) {
        entry = overrideIt->second;
      }
      effective.qualified[qualified] = entry;
      byName[entry.name].push_back(entry);
    }
  }

  for (auto &nameEntry : byName) {
    if (nameEntry.second.size() == 1u) {
      effective.bare[nameEntry.first] = nameEntry.second.front();
      continue;
    }
    std::vector<std::string> sections;
    for (const auto &entry : nameEntry.second) {
      sections.push_back(entry.section);
    }
    std::sort(sections.begin(), sections.end());
    effective.ambiguous[nameEntry.first] = std::move(sections);
  }

  return effective;
}

} // namespace neuron::cli
