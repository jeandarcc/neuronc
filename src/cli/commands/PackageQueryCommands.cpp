#include "neuronc/cli/commands/PackageQueryCommands.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <sstream>
#include <vector>

namespace fs = std::filesystem;

namespace neuron::cli {

namespace {

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

std::string lowerAscii(std::string text) {
  std::transform(text.begin(), text.end(), text.begin(), [](unsigned char ch) {
    return static_cast<char>(std::tolower(ch));
  });
  return text;
}

bool readTextFile(const fs::path &path, std::string *outText) {
  if (outText == nullptr) {
    return false;
  }
  std::ifstream in(path, std::ios::binary);
  if (!in.is_open()) {
    return false;
  }
  std::ostringstream buffer;
  buffer << in.rdbuf();
  *outText = buffer.str();
  return true;
}

std::optional<fs::path> builtinSettingsPath(const fs::path &toolRoot) {
  const std::vector<fs::path> candidates = {
      toolRoot / "config" / "builtin.modulesettings",
      fs::current_path() / "config" / "builtin.modulesettings",
  };
  std::error_code ec;
  for (const auto &candidate : candidates) {
    if (fs::exists(candidate, ec) && fs::is_regular_file(candidate, ec)) {
      return candidate;
    }
    ec.clear();
  }
  return std::nullopt;
}

std::optional<std::string> extractSectionBlock(const std::string &text,
                                               const std::string &sectionName) {
  const std::string target = lowerAscii(sectionName);
  std::size_t offset = 0;
  std::size_t start = std::string::npos;
  std::size_t end = text.size();

  while (offset < text.size()) {
    const std::size_t lineEnd = text.find('\n', offset);
    const std::size_t nextOffset =
        lineEnd == std::string::npos ? text.size() : lineEnd + 1;
    const std::string line = text.substr(offset, nextOffset - offset);
    const std::string trimmed = trimCopy(line);

    if (!trimmed.empty() && trimmed.front() == '[') {
      const std::size_t close = trimmed.find(']');
      if (close != std::string::npos) {
        const std::string current = lowerAscii(trimCopy(trimmed.substr(1, close - 1)));
        if (start != std::string::npos) {
          end = offset;
          break;
        }
        if (current == target) {
          start = offset;
        }
      }
    }

    offset = nextOffset;
  }

  if (start == std::string::npos) {
    return std::nullopt;
  }
  return text.substr(start, end - start);
}

bool resolveBuiltinSettings(const PackageQueryCommandDependencies &deps,
                            const std::string &target, std::string *outText) {
  const auto builtinPath = builtinSettingsPath(deps.toolRoot);
  if (!builtinPath.has_value()) {
    return false;
  }
  std::string text;
  if (!readTextFile(*builtinPath, &text)) {
    return false;
  }
  const auto section = extractSectionBlock(text, target);
  if (!section.has_value()) {
    return false;
  }
  *outText = *section;
  return true;
}

std::string dependencyDescription(const neuron::DependencySpec &dependency) {
  std::vector<std::string> fields;
  if (!dependency.github.empty()) {
    fields.push_back("github = \"" + dependency.github + "\"");
  }
  if (!dependency.version.empty()) {
    fields.push_back("version = \"" + dependency.version + "\"");
  }
  if (!dependency.tag.empty()) {
    fields.push_back("tag = \"" + dependency.tag + "\"");
  }
  if (!dependency.commit.empty()) {
    fields.push_back("commit = \"" + dependency.commit + "\"");
  }
  if (fields.empty()) {
    return "(unspecified)";
  }
  std::ostringstream out;
  for (std::size_t i = 0; i < fields.size(); ++i) {
    if (i != 0) {
      out << ", ";
    }
    out << fields[i];
  }
  return out.str();
}

bool inspectInstalledPackage(const PackageQueryCommandDependencies &deps,
                             const std::string &target,
                             neuron::InstalledPackageDetails *outDetails,
                             std::string *outMessage) {
  if (deps.inspectInstalledPackage) {
    return deps.inspectInstalledPackage(
        deps.currentWorkingDirectory.string(), target, true, outDetails, outMessage);
  }
  return neuron::PackageManager::inspectInstalledPackage(
      deps.currentWorkingDirectory.string(), target, true, outDetails, outMessage);
}

} // namespace

int runSettingsOfCommand(const PackageQueryCommandDependencies &deps,
                         const std::string &target) {
  std::string builtinText;
  if (resolveBuiltinSettings(deps, target, &builtinText)) {
    std::cout << "Builtin module settings for '" << target << "':" << std::endl;
    std::cout << builtinText;
    if (builtinText.empty() || builtinText.back() != '\n') {
      std::cout << std::endl;
    }
    return 0;
  }

  neuron::InstalledPackageDetails details;
  std::string error;
  if (!inspectInstalledPackage(deps, target, &details, &error)) {
    std::cerr << "Error: " << error << std::endl;
    return 1;
  }

  const fs::path settingsPath = details.packageRoot / ".modulesettings";
  std::string text;
  if (!readTextFile(settingsPath, &text)) {
    std::cout << "Package '" << details.locked.name
              << "' has no .modulesettings file." << std::endl;
    return 0;
  }

  std::cout << "Package settings for '" << details.locked.name << "'";
  if (!details.locked.github.empty()) {
    std::cout << " <" << details.locked.github << ">";
  }
  std::cout << ":" << std::endl;
  std::cout << text;
  if (text.empty() || text.back() != '\n') {
    std::cout << std::endl;
  }
  return 0;
}

int runDependenciesOfCommand(const PackageQueryCommandDependencies &deps,
                             const std::string &target) {
  std::string builtinText;
  if (resolveBuiltinSettings(deps, target, &builtinText)) {
    std::cout << "Builtin module '" << target
              << "' has no package dependencies." << std::endl;
    return 0;
  }

  neuron::InstalledPackageDetails details;
  std::string error;
  if (!inspectInstalledPackage(deps, target, &details, &error)) {
    std::cerr << "Error: " << error << std::endl;
    return 1;
  }

  std::cout << "Dependencies for package '" << details.locked.name << "'";
  if (!details.locked.github.empty()) {
    std::cout << " <" << details.locked.github << ">";
  }
  std::cout << ":" << std::endl;

  std::vector<std::string> dependencyNames;
  dependencyNames.reserve(details.config.dependencies.size());
  for (const auto &entry : details.config.dependencies) {
    dependencyNames.push_back(entry.first);
  }
  std::sort(dependencyNames.begin(), dependencyNames.end());

  if (dependencyNames.empty()) {
    std::cout << "  (none)" << std::endl;
  } else {
    for (const auto &name : dependencyNames) {
      const auto &dependency = details.config.dependencies.at(name);
      std::cout << "  - " << name << ": "
                << dependencyDescription(dependency) << std::endl;
    }
  }

  if (!details.locked.transitiveDependencies.empty()) {
    std::vector<std::string> resolved = details.locked.transitiveDependencies;
    std::sort(resolved.begin(), resolved.end());
    std::cout << "Resolved child packages:" << std::endl;
    for (const auto &name : resolved) {
      std::cout << "  - " << name << std::endl;
    }
  }
  return 0;
}

} // namespace neuron::cli
