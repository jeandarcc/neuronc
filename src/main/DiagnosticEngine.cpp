// DiagnosticEngine.cpp — Tanı motoru implementasyonu.
// Bkz. DiagnosticEngine.h
#include "DiagnosticEngine.h"
#include "AppGlobals.h"
#include "UserProfileSettings.h"

#include <algorithm>
#include <filesystem>
#include <iostream>
#include <regex>
#include <sstream>
#include <string>

// ── Dahili yardımcılar ───────────────────────────────────────────────────────

static std::string trimDiag(std::string text) {
  auto notSpace = [](unsigned char c) { return !std::isspace(c); };
  text.erase(text.begin(), std::find_if(text.begin(), text.end(), notSpace));
  text.erase(std::find_if(text.rbegin(), text.rend(), notSpace).base(),
             text.end());
  return text;
}

static bool startsWithCaseInsensitive(const std::string &text,
                                      const std::string &prefix) {
  if (text.size() < prefix.size()) {
    return false;
  }
  for (size_t i = 0; i < prefix.size(); ++i) {
    const char a =
        static_cast<char>(std::tolower(static_cast<unsigned char>(text[i])));
    const char b =
        static_cast<char>(std::tolower(static_cast<unsigned char>(prefix[i])));
    if (a != b) {
      return false;
    }
  }
  return true;
}

static bool looksLikeUnusedVariableMessage(const std::string &message) {
  return startsWithCaseInsensitive(message, "Unused variable:") ||
         startsWithCaseInsensitive(message, "The declared binding is never read.") ||
         startsWithCaseInsensitive(message, "Bildirilen baglama hic okunmuyor.");
}

static DiagnosticSeverity toEngineSeverity(
    neuron::frontend::DiagnosticSeverity severity) {
  return severity == neuron::frontend::DiagnosticSeverity::Warning
             ? DiagnosticSeverity::Warning
             : DiagnosticSeverity::Error;
}

static std::string sourceLineAt(const std::string &source, int lineNumber) {
  if (lineNumber <= 0) {
    return "";
  }
  std::istringstream stream(source);
  std::string line;
  int currentLine = 0;
  while (std::getline(stream, line)) {
    currentLine++;
    if (currentLine == lineNumber) {
      return line;
    }
  }
  return "";
}

static std::string sourceForDiagnostic(
    const std::unordered_map<std::string, std::string> &sourceByFile,
    const std::string &file) {
  const auto it = sourceByFile.find(file);
  return it == sourceByFile.end() ? std::string() : it->second;
}

static void printCaretLine(const std::string &line, int column) {
  if (line.empty()) {
    return;
  }
  std::string caretPadding;
  const int target = std::max(1, column);
  for (int i = 1; i < target; ++i) {
    if (i - 1 < static_cast<int>(line.size()) &&
        line[static_cast<size_t>(i - 1)] == '\t') {
      caretPadding.push_back('\t');
    } else {
      caretPadding.push_back(' ');
    }
  }
  std::cerr << colorize("    " + caretPadding + "^", "\x1b[90m") << std::endl;
}

// ── Renklendirme ─────────────────────────────────────────────────────────────

std::string colorize(const std::string &text, const char *ansiColorCode) {
  if (!g_colorDiagnostics || ansiColorCode == nullptr) {
    return text;
  }
  return std::string(ansiColorCode) + text + "\x1b[0m";
}

// ── Konum ayrıştırma ─────────────────────────────────────────────────────────

DiagnosticLocation parseDiagnosticLocation(const std::string &diagnostic) {
  static const std::regex re(R"(^(.*):([0-9]+):([0-9]+):\s*(.*)$)");
  std::smatch match;
  DiagnosticLocation out;
  if (!std::regex_match(diagnostic, match, re)) {
    out.message = diagnostic;
    return out;
  }
  try {
    out.file    = match[1].str();
    out.line    = std::stoi(match[2].str());
    out.column  = std::stoi(match[3].str());
    out.message = match[4].str();
    out.valid   = true;
  } catch (...) {
    out.message = diagnostic;
  }
  return out;
}

// ── Kod üretimi ──────────────────────────────────────────────────────────────

std::string diagnosticCodeForPhase(const std::string &phase,
                                    DiagnosticSeverity severity) {
  if (severity == DiagnosticSeverity::Warning) {
    return (phase == "config") ? "NR9001" : "NR9000";
  }
  if (phase == "lexer")    { return "NR1001"; }
  if (phase == "parser")   { return "NR1002"; }
  if (phase == "semantic") { return "NR2001"; }
  if (phase == "config")   { return "NR3001"; }
  return "NR0001";
}

// ── Mesaj normalleştirme ─────────────────────────────────────────────────────

std::string normalizeDiagnosticMessage(const std::string &rawMessage,
                                        DiagnosticSeverity *outSeverity) {
  DiagnosticSeverity severity = DiagnosticSeverity::Error;
  std::string message = trimDiag(rawMessage);

  if (startsWithCaseInsensitive(message, "semantic warning:")) {
    message  = trimDiag(message.substr(std::string("semantic warning:").size()));
    severity = DiagnosticSeverity::Warning;
  } else if (startsWithCaseInsensitive(message, "semantic error:")) {
    message = trimDiag(message.substr(std::string("semantic error:").size()));
  } else if (startsWithCaseInsensitive(message, "warning:")) {
    message  = trimDiag(message.substr(std::string("warning:").size()));
    severity = DiagnosticSeverity::Warning;
  } else if (startsWithCaseInsensitive(message, "error:")) {
    message = trimDiag(message.substr(std::string("error:").size()));
  }

  if (outSeverity != nullptr) {
    *outSeverity = severity;
  }
  return message;
}

std::string currentDiagnosticLanguage() {
  const auto settings = neuron::loadUserProfileSettings();
  const std::vector<std::string> supportedLocales =
      neuron::diagnostics::loadSupportedDiagnosticLocales(g_toolRoot);
  const auto resolved = neuron::diagnostics::resolveLanguagePreference(
      settings.has_value() ? settings->language : "auto", supportedLocales);
  return resolved.effective;
}

static neuron::frontend::Diagnostic
localizeDiagnostic(neuron::frontend::Diagnostic diagnostic) {
  neuron::diagnostics::DiagnosticLocalizer localizer(g_toolRoot);
  return localizer.renderDiagnostic(diagnostic, currentDiagnosticLanguage());
}

// ── Yazdırma ─────────────────────────────────────────────────────────────────

void printDiagnostic(const std::string &file, int line, int column,
                      DiagnosticSeverity severity, const std::string &code,
                      const std::string &message) {
  const int safeLine   = std::max(1, line);
  const int safeColumn = std::max(1, column);
  const char *severityLabel =
      severity == DiagnosticSeverity::Warning ? "warning" : "error";
  std::ostringstream stream;
  stream << file << "(" << safeLine << "," << safeColumn
         << "): " << severityLabel << " " << code << ": " << message;
  if (severity == DiagnosticSeverity::Warning) {
    std::cerr << colorize(stream.str(), "\x1b[33m") << std::endl;
  } else {
    std::cerr << colorize(stream.str(), "\x1b[31m") << std::endl;
  }
}

void printTraceLine(const std::string &source, const std::string &file,
                     int line, int column) {
  if (!g_traceErrors) {
    return;
  }
  const int safeLine   = std::max(1, line);
  const int safeColumn = std::max(1, column);
  std::ostringstream location;
  location << "  at <source> in " << file << ":line " << safeLine
            << ", column " << safeColumn;
  std::cerr << colorize(location.str(), "\x1b[90m") << std::endl;
  const std::string sourceLine = sourceLineAt(source, safeLine);
  if (!sourceLine.empty()) {
    std::cerr << colorize("    " + sourceLine, "\x1b[90m") << std::endl;
    printCaretLine(sourceLine, safeColumn);
  }
}

// ── Toplu raporlama ──────────────────────────────────────────────────────────

void reportStringDiagnostics(const std::string &phase,
                               const std::string &filepath,
                               const std::string &source,
                               const std::vector<std::string> &diagnostics) {
  const std::vector<neuron::frontend::Diagnostic> converted =
      neuron::frontend::convertStringDiagnostics(phase, filepath, diagnostics);
  for (auto diagnostic : converted) {
    if (diagnostic.severity == neuron::frontend::DiagnosticSeverity::Warning &&
        phase == "semantic" && looksLikeUnusedVariableMessage(diagnostic.message)) {
      diagnostic.code = "NR9002";
    }
    diagnostic = localizeDiagnostic(std::move(diagnostic));
    const int line = diagnostic.range.start.line;
    const int column = diagnostic.range.start.column;
    printDiagnostic(diagnostic.file, line, column,
                    toEngineSeverity(diagnostic.severity), diagnostic.code,
                    diagnostic.message);
    printTraceLine(source, diagnostic.file, line, column);
  }
}

void reportFrontendDiagnostics(
    const std::vector<neuron::frontend::Diagnostic> &diagnostics,
    const std::unordered_map<std::string, std::string> &sourceByFile) {
  for (auto diagnostic : diagnostics) {
    if (diagnostic.severity == neuron::frontend::DiagnosticSeverity::Warning &&
        diagnostic.phase == "semantic" &&
        looksLikeUnusedVariableMessage(diagnostic.message)) {
      diagnostic.code = "NR9002";
    }
    diagnostic = localizeDiagnostic(std::move(diagnostic));
    const int line = diagnostic.range.start.line;
    const int column = diagnostic.range.start.column;
    printDiagnostic(diagnostic.file, line, column,
                    toEngineSeverity(diagnostic.severity), diagnostic.code,
                    diagnostic.message);
    printTraceLine(sourceForDiagnostic(sourceByFile, diagnostic.file), diagnostic.file,
                   line, column);
  }
}

void reportSemanticDiagnostics(
    const std::string &filepath, const std::string &source,
    const std::vector<neuron::SemanticError> &diagnostics) {
  const std::vector<neuron::frontend::Diagnostic> converted =
      neuron::frontend::convertSemanticDiagnostics(diagnostics);
  for (auto diagnostic : converted) {
    if (diagnostic.severity == neuron::frontend::DiagnosticSeverity::Warning &&
        looksLikeUnusedVariableMessage(diagnostic.message)) {
      diagnostic.code = "NR9002";
    }
    if (diagnostic.file.empty()) {
      diagnostic.file = filepath;
    }
    diagnostic = localizeDiagnostic(std::move(diagnostic));
    const int line = diagnostic.range.start.line;
    const int column = diagnostic.range.start.column;
    printDiagnostic(diagnostic.file, line, column,
                    toEngineSeverity(diagnostic.severity), diagnostic.code,
                    diagnostic.message);
    printTraceLine(source, diagnostic.file, line, column);
  }
}

void reportConfigWarning(const std::string &file, int line,
                          const std::string &message) {
  neuron::frontend::Diagnostic diagnostic;
  diagnostic.phase = "config";
  diagnostic.severity = neuron::frontend::DiagnosticSeverity::Warning;
  diagnostic.code = diagnosticCodeForPhase("config", DiagnosticSeverity::Warning);
  diagnostic.file = file;
  diagnostic.range.start.line = line;
  diagnostic.range.start.column = 1;
  diagnostic.range.end.line = line;
  diagnostic.range.end.column = 2;
  diagnostic.message = message;
  diagnostic = localizeDiagnostic(std::move(diagnostic));
  printDiagnostic(file, line, 1, DiagnosticSeverity::Warning, diagnostic.code,
                  diagnostic.message);
}

