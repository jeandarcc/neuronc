#pragma once

#include "neuronc/cli/ProjectConfig.h"

#include <string>
#include <unordered_map>
#include <vector>

namespace neuron {

struct LockedPackage {
  std::string name;
  std::string github;
  std::string versionConstraint;
  std::string resolvedTag;
  std::string resolvedCommit;
  std::string packageVersion;
  std::string contentHash;
  std::string sourceDir = "src";
  std::vector<std::string> exportedModules;
  std::vector<std::string> transitiveDependencies;
  bool moduleCppEnabled = false;
  std::unordered_map<std::string, ModuleCppConfig> moduleCppModules;
};

struct PackageLock {
  std::unordered_map<std::string, LockedPackage> packages;
};

class PackageLockParser {
public:
  bool parseFile(const std::string &path, PackageLock *outLock);
  bool parseString(const std::string &content, const std::string &sourceName,
                   PackageLock *outLock);

  const std::vector<std::string> &errors() const { return m_errors; }

private:
  enum class Section {
    None,
    Package,
    ModuleCpp,
    Unknown,
  };

  static std::string trim(const std::string &text);
  static std::string stripComment(const std::string &line);
  bool parseLine(const std::string &line, std::size_t lineNumber,
                 Section *section, PackageLock *outLock);
  bool parseKeyValue(const std::string &line, std::size_t lineNumber,
                     std::string *outKey, std::string *outValue);
  bool parseValue(const std::string &rawValue, std::size_t lineNumber,
                  std::string *outValue);
  bool parseStringArray(const std::string &rawValue, std::size_t lineNumber,
                        std::vector<std::string> *outValues);
  void addError(std::size_t lineNumber, const std::string &message);

  std::string m_sourceName;
  std::string m_activePackage;
  std::string m_activeModuleCpp;
  std::vector<std::string> m_errors;
};

bool writePackageLockFile(const std::string &path, const PackageLock &lock,
                          std::string *outError = nullptr);

} // namespace neuron
