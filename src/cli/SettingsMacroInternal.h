#pragma once

#include "neuronc/cli/SettingsMacros.h"

#include <filesystem>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace neuron::cli {

namespace fs = std::filesystem;

struct MacroDefinition {
  std::string section;
  std::string normalizedSection;
  std::string name;
  SettingsMacroKind kind = SettingsMacroKind::Expression;
  std::string rawSnippet;
  fs::path ownerRoot;
  fs::path originFile;
  int originLine = 1;
  int originColumn = 1;
};

struct ProjectOverride {
  std::string section;
  std::string normalizedSection;
  std::string name;
  SettingsMacroKind kind = SettingsMacroKind::Expression;
  std::string rawSnippet;
  int importance = 0;
  fs::path ownerRoot;
  fs::path originFile;
  int originLine = 1;
  int originColumn = 1;
};

struct ProjectSettingsData {
  fs::path root;
  std::unordered_set<std::string> ownedSections;
  std::unordered_map<std::string, std::unordered_map<std::string, MacroDefinition>>
      defaults;
  std::unordered_map<std::string, std::unordered_map<std::string, ProjectOverride>>
      overrides;
};

struct EffectiveMacroEntry {
  std::string section;
  std::string normalizedSection;
  std::string name;
  SettingsMacroKind kind = SettingsMacroKind::Expression;
  std::string rawSnippet;
  int importance = 0;
  fs::path originFile;
  int originLine = 1;
  int originColumn = 1;
};

struct EffectiveMacroSet {
  std::unordered_map<std::string, EffectiveMacroEntry> qualified;
  std::unordered_map<std::string, EffectiveMacroEntry> bare;
  std::unordered_map<std::string, std::vector<std::string>> ambiguous;
};

struct SettingsMacroContext {
  fs::path entryRoot;
  std::vector<fs::path> roots;
  std::unordered_map<std::string, ProjectSettingsData> projects;
  std::unordered_map<std::string, std::vector<std::string>> descendantsByRoot;
  std::unordered_map<std::string, std::vector<std::string>> chainByRoot;
  std::unordered_map<std::string, std::unordered_map<std::string, MacroDefinition>>
      builtinDefaults;
  std::unordered_map<std::string, EffectiveMacroSet> effectiveByOwnerRoot;
};

struct SettingsMacroProcessor::Impl {
  SettingsMacroContext ctx;
};

std::string lowerAscii(std::string value);
std::string trimCopy(const std::string &value);
fs::path normalizePath(const fs::path &path);
std::string pathKey(const fs::path &path);
bool pathStartsWith(const fs::path &path, const fs::path &prefix);
fs::path detectEntryProjectRoot(const fs::path &entrySourcePath);
std::vector<fs::path> discoverProjectRoots(const fs::path &entryRoot);
std::unordered_set<std::string> collectOwnedSectionsForRoot(const fs::path &root);
std::vector<std::string> sourceRootChain(const fs::path &sourcePath,
                                         const std::vector<fs::path> &roots);
std::string qualifiedMacroKey(const std::string &normalizedSection,
                              const std::string &name);
bool isDeclarationLikeContext(const std::vector<Token> &tokens,
                              std::size_t index);
bool readTextFile(const fs::path &path, std::string *outText);
std::string makeConfigError(const fs::path &path, int line, int column,
                            const std::string &message);
std::optional<fs::path> resolveBuiltinSettingsPath(const fs::path &toolRoot);
bool validateProjectOverrides(const SettingsMacroContext &ctx,
                              const std::string &projectRootKey,
                              std::vector<std::string> *outErrors);
EffectiveMacroSet buildEffectiveSetForRoot(const SettingsMacroContext &ctx,
                                           const std::string &ownerRootKey);

class SettingsFileParser {
public:
  SettingsFileParser(fs::path path, std::string text, bool allowImportant);

  bool parse(
      std::unordered_map<std::string, std::unordered_map<std::string, MacroDefinition>>
          *outDefaults,
      std::unordered_map<std::string, std::unordered_map<std::string, ProjectOverride>>
          *outOverrides,
      std::vector<std::string> *outErrors,
      const fs::path &ownerRoot);

private:
  fs::path m_path;
  std::string m_text;
  bool m_allowImportant = false;
  std::size_t m_pos = 0;
  int m_line = 1;
  int m_column = 1;

  char current() const;
  char peek() const;
  bool isAtEnd() const;
  char advance();
  bool match(char expected);
  void skipInlineSpaces();
  void skipLineComment();
  void skipTrivia();
  bool startsWithWord(const std::string &word) const;
  void consumeWord(const std::string &word);
  std::string parseIdentifier();
  bool looksLikeMethodLiteral() const;
  std::string parseExpressionValue(std::vector<std::string> *outErrors);
  std::string parseMethodValue(std::vector<std::string> *outErrors);
  void addError(std::vector<std::string> *outErrors, int line, int column,
                const std::string &message);
};

} // namespace neuron::cli
