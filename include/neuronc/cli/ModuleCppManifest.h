#pragma once

#include <string>
#include <unordered_map>
#include <vector>

namespace neuron {

struct ModuleCppExport {
  std::string name;
  std::string symbol;
  std::vector<std::string> parameterTypes;
  std::string returnType = "void";
};

struct ModuleCppManifest {
  std::string name;
  std::string abi = "c";
  std::unordered_map<std::string, ModuleCppExport> exports;
};

class ModuleCppManifestParser {
public:
  bool parseFile(const std::string &path, ModuleCppManifest *outManifest);
  bool parseString(const std::string &content, const std::string &sourceName,
                   ModuleCppManifest *outManifest);

  const std::vector<std::string> &errors() const { return m_errors; }

private:
  enum class Section {
    None,
    Module,
    Export,
    Unknown,
  };

  static std::string trim(std::string_view text);
  static std::string stripComment(const std::string &line);

  bool parseLine(const std::string &line, std::size_t lineNumber,
                 Section *section, ModuleCppManifest *outManifest);
  bool parseKeyValue(const std::string &line, std::size_t lineNumber,
                     std::string *outKey, std::string *outValue);
  bool parseValue(const std::string &rawValue, std::size_t lineNumber,
                  std::string *outValue);
  bool parseStringArray(const std::string &value,
                        std::vector<std::string> *outValues);
  void addError(std::size_t lineNumber, const std::string &message);

  std::string m_sourceName;
  std::vector<std::string> m_errors;
  std::string m_activeExportName;
};

bool isSupportedModuleCppType(const std::string &typeName);

} // namespace neuron
