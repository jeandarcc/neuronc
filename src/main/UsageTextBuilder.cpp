#include "UsageTextBuilder.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>

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

bool startsWith(const std::string &text, const std::string &prefix) {
  return text.size() >= prefix.size() &&
         text.compare(0, prefix.size(), prefix) == 0;
}

std::optional<std::string> parseQuotedString(const std::string &value) {
  const std::string trimmed = trimCopy(value);
  if (trimmed.size() < 2 || trimmed.front() != '"' || trimmed.back() != '"') {
    return std::nullopt;
  }

  std::string decoded;
  decoded.reserve(trimmed.size() - 2);
  bool escaped = false;
  for (std::size_t i = 1; i + 1 < trimmed.size(); ++i) {
    const char c = trimmed[i];
    if (escaped) {
      switch (c) {
      case 'n':
        decoded.push_back('\n');
        break;
      case 't':
        decoded.push_back('\t');
        break;
      case '\\':
      case '"':
        decoded.push_back(c);
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
    return std::nullopt;
  }
  return decoded;
}

bool parseBool(const std::string &value, bool *out) {
  if (out == nullptr) {
    return false;
  }
  const std::string trimmed = trimCopy(value);
  if (trimmed == "true") {
    *out = true;
    return true;
  }
  if (trimmed == "false") {
    *out = false;
    return true;
  }
  return false;
}

bool parseInt(const std::string &value, int *out) {
  if (out == nullptr) {
    return false;
  }
  const std::string trimmed = trimCopy(value);
  if (trimmed.empty()) {
    return false;
  }
  char *end = nullptr;
  const long parsed = std::strtol(trimmed.c_str(), &end, 10);
  if (end == trimmed.c_str() || *end != '\0') {
    return false;
  }
  *out = static_cast<int>(parsed);
  return true;
}

std::optional<std::vector<std::string>> parseStringArray(const std::string &value) {
  const std::string trimmed = trimCopy(value);
  if (trimmed.size() < 2 || trimmed.front() != '[' || trimmed.back() != ']') {
    return std::nullopt;
  }

  std::vector<std::string> items;
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
      return std::nullopt;
    }
    std::size_t end = cursor + 1;
    bool escaped = false;
    for (; end < trimmed.size(); ++end) {
      const char c = trimmed[end];
      if (escaped) {
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
    }
    if (end >= trimmed.size()) {
      return std::nullopt;
    }
    const auto decoded = parseQuotedString(trimmed.substr(cursor, end - cursor + 1));
    if (!decoded.has_value()) {
      return std::nullopt;
    }
    items.push_back(*decoded);
    cursor = end + 1;
  }
  return items;
}

struct SortByOrder {
  template <typename T>
  bool operator()(const T &lhs, const T &rhs) const {
    if (lhs.order != rhs.order) {
      return lhs.order < rhs.order;
    }
    return lhs.command < rhs.command;
  }
};

} // namespace

std::optional<HelpDocument> loadHelpDocumentFromToml(
    const std::filesystem::path &path) {
  std::ifstream file(path);
  if (!file.is_open()) {
    return std::nullopt;
  }

  HelpDocument document;
  HelpSection *currentSection = nullptr;
  HelpEntry *currentEntry = nullptr;

  std::string line;
  while (std::getline(file, line)) {
    const std::string trimmed = trimCopy(line);
    if (trimmed.empty() || startsWith(trimmed, "#")) {
      continue;
    }

    if (trimmed == "[[section]]") {
      document.sections.emplace_back();
      currentSection = &document.sections.back();
      currentEntry = nullptr;
      continue;
    }

    if (trimmed == "[[section.entry]]") {
      if (currentSection == nullptr) {
        return std::nullopt;
      }
      currentSection->entries.emplace_back();
      currentEntry = &currentSection->entries.back();
      continue;
    }

    const std::size_t equals = trimmed.find('=');
    if (equals == std::string::npos) {
      return std::nullopt;
    }

    const std::string key = trimCopy(trimmed.substr(0, equals));
    const std::string value = trimCopy(trimmed.substr(equals + 1));

    if (currentEntry != nullptr) {
      if (key == "order") {
        if (!parseInt(value, &currentEntry->order)) {
          return std::nullopt;
        }
      } else if (key == "command") {
        const auto parsed = parseQuotedString(value);
        if (!parsed.has_value()) {
          return std::nullopt;
        }
        currentEntry->command = *parsed;
      } else if (key == "detail") {
        const auto parsed = parseQuotedString(value);
        if (!parsed.has_value()) {
          return std::nullopt;
        }
        currentEntry->detail = *parsed;
      } else if (key == "multiline") {
        if (!parseBool(value, &currentEntry->multiline)) {
          return std::nullopt;
        }
      }
      continue;
    }

    if (currentSection != nullptr) {
      if (key == "order") {
        if (!parseInt(value, &currentSection->order)) {
          return std::nullopt;
        }
      } else if (key == "header") {
        const auto parsed = parseQuotedString(value);
        if (!parsed.has_value()) {
          return std::nullopt;
        }
        currentSection->header = *parsed;
      } else if (key == "lines") {
        const auto parsed = parseStringArray(value);
        if (!parsed.has_value()) {
          return std::nullopt;
        }
        currentSection->lines = *parsed;
      }
      continue;
    }

    if (key == "title") {
      const auto parsed = parseQuotedString(value);
      if (!parsed.has_value()) {
        return std::nullopt;
      }
      document.title = *parsed;
    } else if (key == "version") {
      const auto parsed = parseQuotedString(value);
      if (!parsed.has_value()) {
        return std::nullopt;
      }
      document.version = *parsed;
    }
  }

  std::sort(document.sections.begin(), document.sections.end(),
            [](const HelpSection &lhs, const HelpSection &rhs) {
              if (lhs.order != rhs.order) {
                return lhs.order < rhs.order;
              }
              return lhs.header < rhs.header;
            });
  for (HelpSection &section : document.sections) {
    std::sort(section.entries.begin(), section.entries.end(), SortByOrder{});
  }
  return document;
}

std::string renderHelpDocument(const HelpDocument &document) {
  std::ostringstream out;
  out << '\n';

  constexpr std::size_t kCommandColumn = 33;

  for (std::size_t sectionIndex = 0; sectionIndex < document.sections.size();
       ++sectionIndex) {
    const HelpSection &section = document.sections[sectionIndex];

    if (!section.header.empty()) {
      out << "  " << section.header << '\n';
    }

    for (const std::string &line : section.lines) {
      out << line << '\n';
    }

    for (const HelpEntry &entry : section.entries) {
      if (entry.command.empty()) {
        continue;
      }
      out << "    " << entry.command;
      if (!entry.detail.empty()) {
        if (entry.multiline || entry.command.size() >= kCommandColumn) {
          out << '\n' << "                                 " << entry.detail;
        } else {
          const std::size_t padding = kCommandColumn - entry.command.size();
          out << std::string(padding, ' ') << entry.detail;
        }
      }
      out << '\n';
    }

    if (sectionIndex + 1 < document.sections.size()) {
      out << '\n';
    }
  }

  out << "\n";
  return out.str();
}

std::string buildUsageText(const std::filesystem::path &toolRoot) {
  const std::filesystem::path configPath =
      (toolRoot / "config" / "cli" / "help.toml").lexically_normal();
  const auto document = loadHelpDocumentFromToml(configPath);
  if (!document.has_value()) {
    return "Error: CLI help configuration (help.toml) is missing or corrupted.\n";
  }
  return renderHelpDocument(*document);
}

} // namespace neuron::cli

