#include "SettingsMacroInternal.h"

#include <cctype>

namespace neuron::cli {

SettingsFileParser::SettingsFileParser(fs::path path, std::string text,
                                       bool allowImportant)
    : m_path(std::move(path)), m_text(std::move(text)),
      m_allowImportant(allowImportant) {}

bool SettingsFileParser::parse(
    std::unordered_map<std::string, std::unordered_map<std::string, MacroDefinition>>
        *outDefaults,
    std::unordered_map<std::string, std::unordered_map<std::string, ProjectOverride>>
        *outOverrides,
    std::vector<std::string> *outErrors, const fs::path &ownerRoot) {
  std::unordered_set<std::string> seenKeys;
  std::string currentSection;
  std::string currentSectionNormalized;

  while (true) {
    skipTrivia();
    if (isAtEnd()) {
      return outErrors == nullptr || outErrors->empty();
    }

    if (current() == '[') {
      const int sectionLine = m_line;
      const int sectionColumn = m_column;
      advance();
      std::string rawSection;
      while (!isAtEnd() && current() != ']' && current() != '\n') {
        rawSection.push_back(advance());
      }
      if (isAtEnd() || current() != ']') {
        addError(outErrors, sectionLine, sectionColumn,
                 "Unterminated section header.");
        return false;
      }
      advance();
      currentSection = trimCopy(rawSection);
      currentSectionNormalized = lowerAscii(currentSection);
      if (currentSection.empty()) {
        addError(outErrors, sectionLine, sectionColumn,
                 "Empty section name is not allowed.");
        return false;
      }
      continue;
    }

    if (currentSection.empty()) {
      addError(outErrors, m_line, m_column,
               "Settings entry must appear under a section header.");
      return false;
    }

    const int entryLine = m_line;
    const int entryColumn = m_column;
    int importance = 0;
    if (startsWithWord("important")) {
      if (!m_allowImportant) {
        addError(outErrors, entryLine, entryColumn,
                 "'important' is only valid in .projectsettings.");
        return false;
      }
      consumeWord("important");
      importance = 1;
      skipInlineSpaces();
      if (match('(')) {
        skipInlineSpaces();
        std::string digits;
        while (!isAtEnd() &&
               std::isdigit(static_cast<unsigned char>(current())) != 0) {
          digits.push_back(advance());
        }
        skipInlineSpaces();
        if (!match(')')) {
          addError(outErrors, entryLine, entryColumn,
                   "Expected ')' after important(level).");
          return false;
        }
        if (digits.empty()) {
          addError(outErrors, entryLine, entryColumn,
                   "important(level) requires a positive integer.");
          return false;
        }
        try {
          importance = std::stoi(digits);
        } catch (...) {
          importance = 0;
        }
        if (importance < 1) {
          addError(outErrors, entryLine, entryColumn,
                   "important(level) requires level >= 1.");
          return false;
        }
      }
      skipInlineSpaces();
    }

    const std::string key = parseIdentifier();
    if (key.empty()) {
      addError(outErrors, entryLine, entryColumn, "Expected setting key.");
      return false;
    }
    skipInlineSpaces();
    if (!match('=')) {
      addError(outErrors, m_line, m_column, "Expected '=' after setting key.");
      return false;
    }

    skipTrivia();
    const SettingsMacroKind kind =
        looksLikeMethodLiteral() ? SettingsMacroKind::Method
                                 : SettingsMacroKind::Expression;
    const std::string rawValue = kind == SettingsMacroKind::Method
                                     ? parseMethodValue(outErrors)
                                     : parseExpressionValue(outErrors);
    if (outErrors != nullptr && !outErrors->empty()) {
      return false;
    }

    const std::string dedupeKey = currentSectionNormalized + "\n" + key;
    if (!seenKeys.insert(dedupeKey).second) {
      addError(outErrors, entryLine, entryColumn,
               "Duplicate setting '" + currentSection + "." + key +
                   "' in the same file.");
      return false;
    }

    if (m_allowImportant) {
      ProjectOverride entry;
      entry.section = currentSection;
      entry.normalizedSection = currentSectionNormalized;
      entry.name = key;
      entry.kind = kind;
      entry.rawSnippet = trimCopy(rawValue);
      entry.importance = importance;
      entry.ownerRoot = ownerRoot;
      entry.originFile = m_path;
      entry.originLine = entryLine;
      entry.originColumn = entryColumn;
      (*outOverrides)[entry.normalizedSection][entry.name] = std::move(entry);
    } else {
      MacroDefinition entry;
      entry.section = currentSection;
      entry.normalizedSection = currentSectionNormalized;
      entry.name = key;
      entry.kind = kind;
      entry.rawSnippet = trimCopy(rawValue);
      entry.ownerRoot = ownerRoot;
      entry.originFile = m_path;
      entry.originLine = entryLine;
      entry.originColumn = entryColumn;
      (*outDefaults)[entry.normalizedSection][entry.name] = std::move(entry);
    }
  }
}

} // namespace neuron::cli
