#include "neuronc/frontend/Diagnostics.h"

#include <algorithm>
#include <cctype>
#include <optional>
#include <regex>

namespace neuron::frontend {

namespace {

std::string trimCopy(std::string text) {
  auto notSpace = [](unsigned char c) { return !std::isspace(c); };
  text.erase(text.begin(), std::find_if(text.begin(), text.end(), notSpace));
  text.erase(std::find_if(text.rbegin(), text.rend(), notSpace).base(),
             text.end());
  return text;
}

bool startsWithCaseInsensitive(const std::string &text,
                               const std::string &prefix) {
  if (text.size() < prefix.size()) {
    return false;
  }
  for (std::size_t i = 0; i < prefix.size(); ++i) {
    const char lhs =
        static_cast<char>(std::tolower(static_cast<unsigned char>(text[i])));
    const char rhs =
        static_cast<char>(std::tolower(static_cast<unsigned char>(prefix[i])));
    if (lhs != rhs) {
      return false;
    }
  }
  return true;
}

std::optional<neuron::diagnostics::DiagnosticArgument>
parseQuotedSuffixArgument(const std::string &message, const std::string &prefix,
                          const std::string &name) {
  if (message.size() <= prefix.size() || message.rfind(prefix, 0) != 0) {
    return std::nullopt;
  }
  if (message.back() != '\'') {
    return std::nullopt;
  }
  return neuron::diagnostics::DiagnosticArgument{
      name, message.substr(prefix.size(), message.size() - prefix.size() - 1)};
}

void bindSpecificDiagnostic(const std::string &phase, const std::string &message,
                            std::string *code,
                            neuron::diagnostics::DiagnosticArguments *arguments) {
  if (code == nullptr || arguments == nullptr || !code->empty()) {
    return;
  }

  if (phase == "parser") {
    if (const auto token = parseQuotedSuffixArgument(message, "Unexpected token: '",
                                                     "token");
        token.has_value()) {
      *code = "N1103";
      arguments->push_back(*token);
    }
  }
}

std::string phaseDiagnosticCode(const std::string &phase,
                                DiagnosticSeverity severity) {
  if (severity == DiagnosticSeverity::Warning) {
    return phase == "config" ? "NPP9001" : "NPP9000";
  }
  if (phase == "lexer") {
    return "NPP1001";
  }
  if (phase == "parser") {
    return "NPP1002";
  }
  if (phase == "semantic") {
    return "NPP2001";
  }
  if (phase == "config") {
    return "NPP3001";
  }
  if (phase == "module") {
    return "NPP4001";
  }
  return "NPP0001";
}

Range makePointRange(int line, int column) {
  const int safeLine = std::max(1, line);
  const int safeColumn = std::max(1, column);
  return {{safeLine, safeColumn}, {safeLine, safeColumn + 1}};
}

Diagnostic normalizeParsedDiagnostic(const std::string &phase,
                                     const std::string &filepath,
                                     const std::string &file,
                                     int line, int column,
                                     std::string message,
                                     std::string code = {},
                                     neuron::diagnostics::DiagnosticArguments
                                         arguments = {}) {
  Diagnostic diagnostic;
  diagnostic.phase = phase;
  diagnostic.file = file.empty() ? filepath : file;
  diagnostic.range = makePointRange(line, column);
  diagnostic.severity = DiagnosticSeverity::Error;
  message = trimCopy(std::move(message));

  if (startsWithCaseInsensitive(message, "semantic warning:")) {
    diagnostic.severity = DiagnosticSeverity::Warning;
    message = trimCopy(
        message.substr(std::string("semantic warning:").size()));
  } else if (startsWithCaseInsensitive(message, "semantic error:")) {
    message = trimCopy(message.substr(std::string("semantic error:").size()));
  } else if (startsWithCaseInsensitive(message, "warning:")) {
    diagnostic.severity = DiagnosticSeverity::Warning;
    message = trimCopy(message.substr(std::string("warning:").size()));
  } else if (startsWithCaseInsensitive(message, "error:")) {
    message = trimCopy(message.substr(std::string("error:").size()));
  }

  bindSpecificDiagnostic(phase, message, &code, &arguments);
  diagnostic.code =
      code.empty() ? phaseDiagnosticCode(phase, diagnostic.severity) : std::move(code);
  diagnostic.message = std::move(message);
  diagnostic.arguments = std::move(arguments);
  return diagnostic;
}

} // namespace

std::vector<Diagnostic>
convertStringDiagnostics(const std::string &phase, const std::string &filepath,
                         const std::vector<std::string> &diagnostics) {
  static const std::regex pattern(R"(^(.*):([0-9]+):([0-9]+):\s*(.*)$)");
  std::vector<Diagnostic> converted;
  converted.reserve(diagnostics.size());

  for (const std::string &entry : diagnostics) {
    std::smatch match;
    if (std::regex_match(entry, match, pattern)) {
      converted.push_back(normalizeParsedDiagnostic(
          phase, filepath, match[1].str(), std::stoi(match[2].str()),
          std::stoi(match[3].str()), match[4].str()));
      continue;
    }

    converted.push_back(
        normalizeParsedDiagnostic(phase, filepath, filepath, 1, 1, entry));
  }

  return converted;
}

std::vector<Diagnostic>
convertParserDiagnostics(const std::string &filepath,
                         const std::vector<neuron::ParserDiagnostic> &diagnostics) {
  std::vector<Diagnostic> converted;
  converted.reserve(diagnostics.size());
  for (const auto &entry : diagnostics) {
    converted.push_back(normalizeParsedDiagnostic(
        "parser", filepath, entry.location.file, entry.location.line,
        entry.location.column, entry.detail, entry.code, entry.arguments));
  }
  return converted;
}

std::vector<Diagnostic>
convertSemanticDiagnostics(const std::vector<neuron::SemanticError> &diagnostics) {
  std::vector<Diagnostic> converted;
  converted.reserve(diagnostics.size());
  for (const auto &entry : diagnostics) {
    converted.push_back(normalizeParsedDiagnostic(
        "semantic", entry.location.file, entry.location.file, entry.location.line,
        entry.location.column, entry.message, entry.code, entry.arguments));
  }
  return converted;
}

} // namespace neuron::frontend
