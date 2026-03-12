#include "neuronc/diagnostics/DiagnosticLocalizer.h"

#include "neuronc/diagnostics/DiagnosticLocale.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <functional>
#include <optional>
#include <string>
#include <unordered_set>

namespace neuron::diagnostics {

namespace {

std::string trimCopy(std::string text) {
  auto notSpace = [](unsigned char ch) { return !std::isspace(ch); };
  text.erase(text.begin(), std::find_if(text.begin(), text.end(), notSpace));
  text.erase(std::find_if(text.rbegin(), text.rend(), notSpace).base(),
             text.end());
  return text;
}

std::string unquote(std::string value) {
  value = trimCopy(std::move(value));
  if (value.size() >= 2 && value.front() == '"' && value.back() == '"') {
    return value.substr(1, value.size() - 2);
  }
  return value;
}

bool hasTomlExtension(const std::filesystem::path &path) {
  return path.extension() == ".toml";
}

std::vector<std::string> parseStringArray(std::string value) {
  std::vector<std::string> values;
  value = trimCopy(std::move(value));
  if (value.size() < 2 || value.front() != '[' || value.back() != ']') {
    return values;
  }

  std::string current;
  bool inString = false;
  for (std::size_t i = 1; i + 1 < value.size(); ++i) {
    const char ch = value[i];
    if (ch == '"') {
      inString = !inString;
      continue;
    }
    if (!inString) {
      if (ch == ',') {
        if (!current.empty()) {
          values.push_back(current);
          current.clear();
        }
        continue;
      }
      if (std::isspace(static_cast<unsigned char>(ch))) {
        continue;
      }
    }
    current.push_back(ch);
  }
  if (!current.empty()) {
    values.push_back(current);
  }
  return values;
}

std::string fallbackCodeFor(const frontend::Diagnostic &diagnostic) {
  if (diagnostic.severity == frontend::DiagnosticSeverity::Warning) {
    return diagnostic.phase == "config" ? "NPP9001" : "NPP9000";
  }
  if (diagnostic.phase == "lexer") {
    return "NPP1001";
  }
  if (diagnostic.phase == "parser") {
    return "NPP1002";
  }
  if (diagnostic.phase == "semantic") {
    return "NPP2001";
  }
  if (diagnostic.phase == "config") {
    return "NPP3001";
  }
  if (diagnostic.phase == "module") {
    return "NPP4001";
  }
  return "NPP0001";
}

std::optional<std::string>
renderTemplate(const LocalizedDiagnosticEntry &entry,
               const DiagnosticArguments &arguments) {
  if (entry.defaultMessage.empty()) {
    return std::nullopt;
  }

  if (!entry.parameters.empty()) {
    for (const auto &name : entry.parameters) {
      if (!findDiagnosticArgument(arguments, name).has_value()) {
        return std::nullopt;
      }
    }
  }

  std::string rendered;
  rendered.reserve(entry.defaultMessage.size() + 32);
  for (std::size_t i = 0; i < entry.defaultMessage.size(); ++i) {
    const char ch = entry.defaultMessage[i];
    if (ch != '{') {
      rendered.push_back(ch);
      continue;
    }

    const std::size_t close = entry.defaultMessage.find('}', i + 1);
    if (close == std::string::npos) {
      return std::nullopt;
    }
    const std::string name = entry.defaultMessage.substr(i + 1, close - i - 1);
    const auto value = findDiagnosticArgument(arguments, name);
    if (!value.has_value()) {
      return std::nullopt;
    }
    rendered.append(value->begin(), value->end());
    i = close;
  }
  return rendered;
}

} // namespace

DiagnosticLocalizer::DiagnosticLocalizer(std::filesystem::path toolRoot)
    : m_toolRoot(std::move(toolRoot)) {}

void DiagnosticLocalizer::setToolRoot(std::filesystem::path toolRoot) {
  if (m_toolRoot != toolRoot) {
    m_toolRoot = std::move(toolRoot);
    m_cache.clear();
  }
}

const std::filesystem::path &DiagnosticLocalizer::toolRoot() const {
  return m_toolRoot;
}

std::optional<std::reference_wrapper<const DiagnosticLocalizer::EntryMap>>
DiagnosticLocalizer::loadLocaleEntries(const std::string &languageCode) {
  const std::string normalized = normalizeLanguageCode(languageCode);
  auto cached = m_cache.find(normalized);
  if (cached != m_cache.end()) {
    return std::cref(cached->second);
  }

  EntryMap entries;
  const std::filesystem::path localeDir =
      m_toolRoot / "config" / "diagnostics" / normalized;
  std::error_code ec;
  if (!std::filesystem::exists(localeDir, ec) ||
      !std::filesystem::is_directory(localeDir, ec)) {
    m_cache.emplace(normalized, std::move(entries));
    return std::cref(m_cache.find(normalized)->second);
  }

  std::vector<std::filesystem::path> files;
  for (std::filesystem::directory_iterator it(localeDir, ec), end; it != end;
       it.increment(ec)) {
    if (ec || !it->is_regular_file() || !hasTomlExtension(it->path())) {
      continue;
    }
    files.push_back(it->path());
  }
  std::sort(files.begin(), files.end());

  for (const auto &file : files) {
    std::ifstream input(file);
    if (!input.is_open()) {
      continue;
    }

    std::string line;
    std::string currentCode;
    while (std::getline(input, line)) {
      const std::size_t comment = line.find('#');
      if (comment != std::string::npos) {
        line = line.substr(0, comment);
      }
      line = trimCopy(line);
      if (line.empty()) {
        continue;
      }

      if (line.rfind("[messages.", 0) == 0 && line.back() == ']') {
        currentCode = line.substr(std::string("[messages.").size());
        currentCode.pop_back();
        entries[currentCode].code = currentCode;
        continue;
      }

      if (currentCode.empty()) {
        continue;
      }

      const std::size_t eq = line.find('=');
      if (eq == std::string::npos) {
        continue;
      }

      const std::string key = trimCopy(line.substr(0, eq));
      const std::string value = unquote(line.substr(eq + 1));
      auto &entry = entries[currentCode];
      if (key == "title") {
        entry.title = value;
      } else if (key == "summary") {
        entry.summary = value;
      } else if (key == "default_message") {
        entry.defaultMessage = value;
      } else if (key == "recovery") {
        entry.recovery = value;
      } else if (key == "parameters") {
        entry.parameters = parseStringArray(line.substr(eq + 1));
      }
    }
  }

  auto [it, inserted] = m_cache.emplace(normalized, std::move(entries));
  (void)inserted;
  return std::cref(it->second);
}

std::optional<LocalizedDiagnosticEntry>
DiagnosticLocalizer::findEntry(const std::string &languageCode,
                               const std::string &diagnosticCode) {
  const auto entries = loadLocaleEntries(languageCode);
  if (!entries.has_value()) {
    return std::nullopt;
  }
  const auto it = entries->get().find(diagnosticCode);
  if (it == entries->get().end()) {
    return std::nullopt;
  }
  return it->second;
}

frontend::Diagnostic
DiagnosticLocalizer::renderDiagnostic(const frontend::Diagnostic &diagnostic,
                                      const std::string &languageCode) {
  frontend::Diagnostic rendered = diagnostic;

  auto tryRender = [&](const std::string &code) -> std::optional<std::string> {
    const auto entry = findEntry(languageCode, code);
    if (!entry.has_value()) {
      return std::nullopt;
    }
    return renderTemplate(*entry, diagnostic.arguments);
  };

  if (!rendered.code.empty()) {
    if (const auto message = tryRender(rendered.code); message.has_value()) {
      rendered.message = std::move(*message);
      return rendered;
    }
  }

  rendered.code = fallbackCodeFor(diagnostic);
  if (const auto message = tryRender(rendered.code); message.has_value()) {
    rendered.message = std::move(*message);
    rendered.arguments.clear();
    return rendered;
  }

  rendered.message = rendered.code;
  rendered.arguments.clear();
  return rendered;
}

std::vector<frontend::Diagnostic> DiagnosticLocalizer::localizeDiagnostics(
    const std::vector<frontend::Diagnostic> &diagnostics,
    const std::string &languageCode) {
  std::vector<frontend::Diagnostic> localized = diagnostics;
  for (auto &diagnostic : localized) {
    diagnostic = renderDiagnostic(diagnostic, languageCode);
  }
  return localized;
}

} // namespace neuron::diagnostics

