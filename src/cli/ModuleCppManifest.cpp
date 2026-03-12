#include "neuronc/cli/ModuleCppManifest.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>

namespace neuron {

namespace {

std::string toLowerCopy(const std::string &text) {
  std::string lowered = text;
  std::transform(lowered.begin(), lowered.end(), lowered.begin(),
                 [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  return lowered;
}

} // namespace

bool isSupportedModuleCppType(const std::string &typeName) {
  const std::string lowered = toLowerCopy(typeName);
  return lowered == "void" || lowered == "int" || lowered == "float" ||
         lowered == "double" || lowered == "bool" || lowered == "string";
}

bool ModuleCppManifestParser::parseFile(const std::string &path,
                                        ModuleCppManifest *outManifest) {
  std::ifstream file(path);
  if (!file.is_open()) {
    m_errors.clear();
    m_sourceName = path;
    addError(0, "could not open file");
    return false;
  }

  std::ostringstream content;
  content << file.rdbuf();
  return parseString(content.str(), path, outManifest);
}

bool ModuleCppManifestParser::parseString(const std::string &content,
                                          const std::string &sourceName,
                                          ModuleCppManifest *outManifest) {
  m_sourceName = sourceName;
  m_errors.clear();
  m_activeExportName.clear();

  if (outManifest == nullptr) {
    addError(0, "output manifest pointer is null");
    return false;
  }

  *outManifest = ModuleCppManifest{};
  Section section = Section::None;

  std::istringstream stream(content);
  std::string line;
  std::size_t lineNumber = 0;
  while (std::getline(stream, line)) {
    ++lineNumber;
    parseLine(line, lineNumber, &section, outManifest);
  }

  if (outManifest->name.empty()) {
    addError(0, "missing required key: [module].name");
  }
  if (outManifest->exports.empty()) {
    addError(0, "modulecpp manifest must define at least one [export.<Name>] section");
  }
  for (const auto &entry : outManifest->exports) {
    if (entry.second.symbol.empty()) {
      addError(0, "modulecpp export '" + entry.first + "' is missing a symbol");
    }
    for (const auto &paramType : entry.second.parameterTypes) {
      if (!isSupportedModuleCppType(paramType)) {
        addError(0, "unsupported modulecpp type in export '" + entry.first +
                        "': " + paramType);
      }
    }
    if (!isSupportedModuleCppType(entry.second.returnType)) {
      addError(0, "unsupported modulecpp return type in export '" + entry.first +
                      "': " + entry.second.returnType);
    }
  }

  return m_errors.empty();
}

std::string ModuleCppManifestParser::trim(std::string_view text) {
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
  return std::string(text.substr(begin, end - begin));
}

std::string ModuleCppManifestParser::stripComment(const std::string &line) {
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

bool ModuleCppManifestParser::parseLine(const std::string &line,
                                        std::size_t lineNumber,
                                        Section *section,
                                        ModuleCppManifest *outManifest) {
  const std::string clean = trim(stripComment(line));
  if (clean.empty()) {
    return true;
  }

  if (clean.front() == '[') {
    if (clean.back() != ']') {
      addError(lineNumber, "invalid section header");
      return false;
    }
    const std::string rawSectionName = trim(
        std::string_view(clean.data() + 1, clean.size() - 2));
    const std::string sectionName = toLowerCopy(rawSectionName);
    m_activeExportName.clear();
    if (sectionName == "module") {
      *section = Section::Module;
    } else if (sectionName.rfind("export.", 0) == 0) {
      m_activeExportName = rawSectionName.substr(std::string("export.").size());
      if (m_activeExportName.empty()) {
        addError(lineNumber, "empty export section name");
        return false;
      }
      auto &entry = outManifest->exports[m_activeExportName];
      entry.name = m_activeExportName;
      *section = Section::Export;
    } else {
      *section = Section::Unknown;
    }
    return true;
  }

  std::string key;
  std::string value;
  if (!parseKeyValue(clean, lineNumber, &key, &value)) {
    return false;
  }

  switch (*section) {
  case Section::Module:
    if (key == "name") {
      outManifest->name = value;
    } else if (key == "abi") {
      outManifest->abi = value;
    } else {
      addError(lineNumber, "unknown module key: " + key);
      return false;
    }
    break;
  case Section::Export: {
    if (m_activeExportName.empty()) {
      addError(lineNumber, "export key declared outside of export section");
      return false;
    }
    auto &entry = outManifest->exports[m_activeExportName];
    if (key == "symbol") {
      entry.symbol = value;
    } else if (key == "params") {
      if (!parseStringArray(value, &entry.parameterTypes)) {
        addError(lineNumber, "invalid export.params value (expected [\"...\"])");
        return false;
      }
    } else if (key == "return") {
      entry.returnType = value;
    } else {
      addError(lineNumber, "unknown export key: " + key);
      return false;
    }
    break;
  }
  case Section::None:
    addError(lineNumber, "key-value pair found outside of a section");
    return false;
  case Section::Unknown:
    break;
  }

  return true;
}

bool ModuleCppManifestParser::parseKeyValue(const std::string &line,
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

bool ModuleCppManifestParser::parseValue(const std::string &rawValue,
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
  decoded.reserve(rawValue.size() - 2);
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
      continue;
    }
    decoded.push_back(c);
  }

  if (escaped) {
    addError(lineNumber, "invalid trailing escape in string value");
    return false;
  }

  *outValue = std::move(decoded);
  return true;
}

bool ModuleCppManifestParser::parseStringArray(
    const std::string &value, std::vector<std::string> *outValues) {
  if (outValues == nullptr) {
    return false;
  }
  outValues->clear();

  const std::string trimmed = trim(value);
  if (trimmed.size() < 2 || trimmed.front() != '[' || trimmed.back() != ']') {
    return false;
  }

  std::size_t cursor = 1;
  while (cursor + 1 < trimmed.size()) {
    while (cursor + 1 < trimmed.size() &&
           (std::isspace(static_cast<unsigned char>(trimmed[cursor])) != 0 ||
            trimmed[cursor] == ',')) {
      ++cursor;
    }
    if (cursor + 1 >= trimmed.size()) {
      break;
    }
    if (trimmed[cursor] != '"') {
      return false;
    }
    ++cursor;

    std::string entry;
    bool escaped = false;
    while (cursor + 1 < trimmed.size()) {
      const char c = trimmed[cursor++];
      if (escaped) {
        entry.push_back(c);
        escaped = false;
        continue;
      }
      if (c == '\\') {
        escaped = true;
        continue;
      }
      if (c == '"') {
        break;
      }
      entry.push_back(c);
    }
    if (escaped || cursor > trimmed.size()) {
      outValues->clear();
      return false;
    }
    outValues->push_back(std::move(entry));
  }

  return true;
}

void ModuleCppManifestParser::addError(std::size_t lineNumber,
                                       const std::string &message) {
  std::ostringstream error;
  if (lineNumber == 0) {
    error << m_sourceName << ": error: " << message;
  } else {
    error << m_sourceName << ":" << lineNumber << ": error: " << message;
  }
  m_errors.push_back(error.str());
}

} // namespace neuron
