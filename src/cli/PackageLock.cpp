#include "neuronc/cli/PackageLock.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>

namespace neuron {

namespace {

std::string toLowerCopy(const std::string &text) {
  std::string lowered = text;
  std::transform(lowered.begin(), lowered.end(), lowered.begin(),
                 [](unsigned char c) {
                   return static_cast<char>(std::tolower(c));
                 });
  return lowered;
}

void writeQuotedStringArray(std::ofstream &out, const std::string &key,
                            const std::vector<std::string> &values) {
  out << key << " = [";
  for (size_t i = 0; i < values.size(); ++i) {
    if (i != 0) {
      out << ", ";
    }
    out << "\"" << values[i] << "\"";
  }
  out << "]\n";
}

} // namespace

bool PackageLockParser::parseFile(const std::string &path, PackageLock *outLock) {
  std::ifstream file(path);
  if (!file.is_open()) {
    m_errors.clear();
    m_sourceName = path;
    addError(0, "could not open file");
    return false;
  }

  std::ostringstream content;
  content << file.rdbuf();
  return parseString(content.str(), path, outLock);
}

bool PackageLockParser::parseString(const std::string &content,
                                    const std::string &sourceName,
                                    PackageLock *outLock) {
  m_sourceName = sourceName;
  m_activePackage.clear();
  m_activeModuleCpp.clear();
  m_errors.clear();

  if (outLock == nullptr) {
    addError(0, "output lock pointer is null");
    return false;
  }

  *outLock = PackageLock{};
  Section section = Section::None;

  std::istringstream stream(content);
  std::string line;
  std::size_t lineNumber = 0;
  while (std::getline(stream, line)) {
    ++lineNumber;
    parseLine(line, lineNumber, &section, outLock);
  }

  return m_errors.empty();
}

std::string PackageLockParser::trim(const std::string &text) {
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

std::string PackageLockParser::stripComment(const std::string &line) {
  bool inString = false;
  bool escaped = false;
  for (std::size_t i = 0; i < line.size(); ++i) {
    const char c = line[i];
    if (escaped) {
      escaped = false;
      continue;
    }
    if (c == '\\' && inString) {
      escaped = true;
      continue;
    }
    if (c == '"') {
      inString = !inString;
      continue;
    }
    if (c == '#' && !inString) {
      return line.substr(0, i);
    }
  }
  return line;
}

bool PackageLockParser::parseLine(const std::string &line,
                                  std::size_t lineNumber, Section *section,
                                  PackageLock *outLock) {
  const std::string clean = trim(stripComment(line));
  if (clean.empty()) {
    return true;
  }

  if (clean.front() == '[') {
    if (clean.back() != ']') {
      addError(lineNumber, "invalid section header");
      return false;
    }

    const std::string rawSectionName =
        trim(clean.substr(1, clean.size() - 2));
    const std::string sectionName = toLowerCopy(rawSectionName);
    m_activePackage.clear();
    m_activeModuleCpp.clear();
    if (sectionName.rfind("package.", 0) == 0) {
      const std::string suffix = rawSectionName.substr(std::string("package.").size());
      const std::string loweredSuffix = toLowerCopy(suffix);
      const std::string moduleCppMarker = ".modulecpp.";
      const std::size_t moduleCppPos = loweredSuffix.find(moduleCppMarker);
      if (moduleCppPos == std::string::npos) {
        m_activePackage = suffix;
        *section = Section::Package;
      } else {
        m_activePackage = suffix.substr(0, moduleCppPos);
        m_activeModuleCpp =
            suffix.substr(moduleCppPos + moduleCppMarker.size());
        if (m_activePackage.empty() || m_activeModuleCpp.empty()) {
          addError(lineNumber, "invalid package.modulecpp section");
          return false;
        }
        *section = Section::ModuleCpp;
      }
      return true;
    }

    *section = Section::Unknown;
    return true;
  }

  std::string key;
  std::string value;
  if (!parseKeyValue(clean, lineNumber, &key, &value)) {
    return false;
  }

  switch (*section) {
  case Section::Package: {
    LockedPackage &locked = outLock->packages[m_activePackage];
    locked.name = m_activePackage;
    if (key == "github") {
      locked.github = value;
    } else if (key == "version") {
      locked.versionConstraint = value;
    } else if (key == "resolved_tag") {
      locked.resolvedTag = value;
    } else if (key == "resolved_commit") {
      locked.resolvedCommit = value;
    } else if (key == "package_version") {
      locked.packageVersion = value;
    } else if (key == "content_hash") {
      locked.contentHash = value;
    } else if (key == "source_dir") {
      locked.sourceDir = value;
    } else if (key == "modulecpp_enabled") {
      locked.moduleCppEnabled = (toLowerCopy(value) == "true");
    } else if (key == "exported_modules") {
      if (!parseStringArray(value, lineNumber, &locked.exportedModules)) {
        return false;
      }
    } else if (key == "transitive_dependencies") {
      if (!parseStringArray(value, lineNumber, &locked.transitiveDependencies)) {
        return false;
      }
    } else {
      addError(lineNumber, "unknown package lock key: " + key);
      return false;
    }
    return true;
  }
  case Section::ModuleCpp: {
    if (m_activePackage.empty() || m_activeModuleCpp.empty()) {
      addError(lineNumber, "package lock modulecpp section missing names");
      return false;
    }
    LockedPackage &locked = outLock->packages[m_activePackage];
    locked.name = m_activePackage;
    ModuleCppConfig &config = locked.moduleCppModules[m_activeModuleCpp];
    if (key == "manifest") {
      config.manifestPath = value;
    } else if (key == "build_system") {
      config.buildSystem = value;
    } else if (key == "source_dir") {
      config.sourceDir = value;
    } else if (key == "cmake_target") {
      config.cmakeTarget = value;
    } else if (key == "artifact_windows_x64") {
      config.artifactWindowsX64 = value;
    } else if (key == "artifact_linux_x64") {
      config.artifactLinuxX64 = value;
    } else if (key == "artifact_macos_arm64") {
      config.artifactMacosArm64 = value;
    } else {
      addError(lineNumber, "unknown package lock modulecpp key: " + key);
      return false;
    }
    return true;
  }
  case Section::None:
    addError(lineNumber, "key-value pair found outside of a section");
    return false;
  case Section::Unknown:
    return true;
  }

  return true;
}

bool PackageLockParser::parseKeyValue(const std::string &line,
                                      std::size_t lineNumber,
                                      std::string *outKey,
                                      std::string *outValue) {
  bool inString = false;
  bool escaped = false;
  std::size_t separator = std::string::npos;

  for (std::size_t i = 0; i < line.size(); ++i) {
    const char c = line[i];
    if (escaped) {
      escaped = false;
      continue;
    }
    if (c == '\\' && inString) {
      escaped = true;
      continue;
    }
    if (c == '"') {
      inString = !inString;
      continue;
    }
    if (c == '=' && !inString) {
      separator = i;
      break;
    }
  }

  if (separator == std::string::npos) {
    addError(lineNumber, "expected '=' in key-value pair");
    return false;
  }

  *outKey = trim(line.substr(0, separator));
  if (outKey->empty()) {
    addError(lineNumber, "empty key in key-value pair");
    return false;
  }

  return parseValue(trim(line.substr(separator + 1)), lineNumber, outValue);
}

bool PackageLockParser::parseValue(const std::string &rawValue,
                                   std::size_t lineNumber,
                                   std::string *outValue) {
  if (rawValue.empty()) {
    addError(lineNumber, "empty value in key-value pair");
    return false;
  }

  if (rawValue.front() != '"') {
    *outValue = rawValue;
    return true;
  }

  if (rawValue.size() < 2 || rawValue.back() != '"') {
    addError(lineNumber, "unterminated string value");
    return false;
  }

  std::string decoded;
  bool escaped = false;
  for (std::size_t i = 1; i + 1 < rawValue.size(); ++i) {
    const char c = rawValue[i];
    if (escaped) {
      switch (c) {
      case 'n':
        decoded.push_back('\n');
        break;
      case 'r':
        decoded.push_back('\r');
        break;
      case 't':
        decoded.push_back('\t');
        break;
      case '"':
        decoded.push_back('"');
        break;
      case '\\':
        decoded.push_back('\\');
        break;
      default:
        decoded.push_back(c);
        break;
      }
      escaped = false;
      continue;
    }
    if (c == '\\') {
      escaped = true;
    } else {
      decoded.push_back(c);
    }
  }

  if (escaped) {
    addError(lineNumber, "invalid trailing escape in string value");
    return false;
  }

  *outValue = std::move(decoded);
  return true;
}

bool PackageLockParser::parseStringArray(const std::string &rawValue,
                                         std::size_t lineNumber,
                                         std::vector<std::string> *outValues) {
  if (outValues == nullptr) {
    addError(lineNumber, "internal error: null string-array output");
    return false;
  }
  outValues->clear();

  const std::string trimmed = trim(rawValue);
  if (trimmed.size() < 2 || trimmed.front() != '[' || trimmed.back() != ']') {
    addError(lineNumber, "invalid string array value");
    return false;
  }

  std::string current;
  bool inString = false;
  bool escaped = false;
  for (std::size_t i = 1; i + 1 < trimmed.size(); ++i) {
    const char c = trimmed[i];
    if (escaped) {
      current.push_back(c);
      escaped = false;
      continue;
    }
    if (c == '\\' && inString) {
      escaped = true;
      continue;
    }
    if (c == '"') {
      inString = !inString;
      continue;
    }
    if (c == ',' && !inString) {
      outValues->push_back(trim(current));
      current.clear();
      continue;
    }
    current.push_back(c);
  }
  if (!trim(current).empty()) {
    outValues->push_back(trim(current));
  }

  for (std::string &item : *outValues) {
    item = trim(item);
  }

  return true;
}

void PackageLockParser::addError(std::size_t lineNumber,
                                 const std::string &message) {
  std::ostringstream error;
  if (lineNumber == 0) {
    error << m_sourceName << ": error: " << message;
  } else {
    error << m_sourceName << ":" << lineNumber << ": error: " << message;
  }
  m_errors.push_back(error.str());
}

bool writePackageLockFile(const std::string &path, const PackageLock &lock,
                          std::string *outError) {
  std::ofstream out(path);
  if (!out.is_open()) {
    if (outError != nullptr) {
      *outError = "failed to open package lock for writing: " + path;
    }
    return false;
  }

  std::vector<std::string> packageNames;
  packageNames.reserve(lock.packages.size());
  for (const auto &entry : lock.packages) {
    packageNames.push_back(entry.first);
  }
  std::sort(packageNames.begin(), packageNames.end());

  for (size_t i = 0; i < packageNames.size(); ++i) {
    const LockedPackage &locked = lock.packages.at(packageNames[i]);
    out << "[package." << locked.name << "]\n";
    if (!locked.github.empty()) {
      out << "github = \"" << locked.github << "\"\n";
    }
    if (!locked.versionConstraint.empty()) {
      out << "version = \"" << locked.versionConstraint << "\"\n";
    }
    if (!locked.resolvedTag.empty()) {
      out << "resolved_tag = \"" << locked.resolvedTag << "\"\n";
    }
    if (!locked.resolvedCommit.empty()) {
      out << "resolved_commit = \"" << locked.resolvedCommit << "\"\n";
    }
    if (!locked.packageVersion.empty()) {
      out << "package_version = \"" << locked.packageVersion << "\"\n";
    }
    if (!locked.contentHash.empty()) {
      out << "content_hash = \"" << locked.contentHash << "\"\n";
    }
    out << "source_dir = \"" << locked.sourceDir << "\"\n";
    out << "modulecpp_enabled = "
        << (locked.moduleCppEnabled ? "true" : "false") << "\n";
    writeQuotedStringArray(out, "exported_modules", locked.exportedModules);
    writeQuotedStringArray(out, "transitive_dependencies",
                           locked.transitiveDependencies);
    out << "\n";

    std::vector<std::string> moduleNames;
    moduleNames.reserve(locked.moduleCppModules.size());
    for (const auto &moduleEntry : locked.moduleCppModules) {
      moduleNames.push_back(moduleEntry.first);
    }
    std::sort(moduleNames.begin(), moduleNames.end());
    for (const auto &moduleName : moduleNames) {
      const ModuleCppConfig &config = locked.moduleCppModules.at(moduleName);
      out << "[package." << locked.name << ".modulecpp." << moduleName << "]\n";
      if (!config.manifestPath.empty()) {
        out << "manifest = \"" << config.manifestPath << "\"\n";
      }
      if (!config.buildSystem.empty()) {
        out << "build_system = \"" << config.buildSystem << "\"\n";
      }
      if (!config.sourceDir.empty()) {
        out << "source_dir = \"" << config.sourceDir << "\"\n";
      }
      if (!config.cmakeTarget.empty()) {
        out << "cmake_target = \"" << config.cmakeTarget << "\"\n";
      }
      if (!config.artifactWindowsX64.empty()) {
        out << "artifact_windows_x64 = \"" << config.artifactWindowsX64
            << "\"\n";
      }
      if (!config.artifactLinuxX64.empty()) {
        out << "artifact_linux_x64 = \"" << config.artifactLinuxX64 << "\"\n";
      }
      if (!config.artifactMacosArm64.empty()) {
        out << "artifact_macos_arm64 = \"" << config.artifactMacosArm64
            << "\"\n";
      }
      out << "\n";
    }
  }

  if (!out.good()) {
    if (outError != nullptr) {
      *outError = "failed while writing package lock: " + path;
    }
    return false;
  }

  return true;
}

} // namespace neuron
