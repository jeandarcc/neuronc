// SettingsLoader.cpp — .neuronsettings yükleme ve politika doğrulama
// implementasyonu. Bkz. SettingsLoader.h
#include "SettingsLoader.h"
#include "AppGlobals.h"
#include "DiagnosticEngine.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

#ifdef _WIN32
#include <io.h>
#include <windows.h>
#else
#include <unistd.h>
#endif

// ── Internal string helpers ──────────────────────────────────────────────────

static std::string trimStr(std::string text) {
  auto notSpace = [](unsigned char c) { return !std::isspace(c); };
  text.erase(text.begin(), std::find_if(text.begin(), text.end(), notSpace));
  text.erase(std::find_if(text.rbegin(), text.rend(), notSpace).base(),
             text.end());
  return text;
}

static bool parsePositiveOrZeroInt(const std::string &text, int *outValue) {
  if (outValue == nullptr) {
    return false;
  }
  try {
    std::size_t consumed = 0;
    const int parsed = std::stoi(text, &consumed);
    if (consumed != text.size() || parsed < 0) {
      return false;
    }
    *outValue = parsed;
    return true;
  } catch (...) {
    return false;
  }
}

static bool parseBoolSetting(const std::string &text, bool *outValue) {
  if (outValue == nullptr) {
    return false;
  }
  std::string value = text;
  std::transform(
      value.begin(), value.end(), value.begin(),
      [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  if (value == "true" || value == "1" || value == "yes" || value == "on") {
    *outValue = true;
    return true;
  }
  if (value == "false" || value == "0" || value == "no" || value == "off") {
    *outValue = false;
    return true;
  }
  return false;
}

static bool parseStringListSetting(const std::string &text,
                                   std::vector<std::string> *outValues) {
  if (outValues == nullptr) {
    return false;
  }
  std::string value = trimStr(text);
  if (value.size() < 2 || value.front() != '[' || value.back() != ']') {
    return false;
  }
  value = trimStr(value.substr(1, value.size() - 2));
  outValues->clear();
  if (value.empty()) {
    return true;
  }

  std::string current;
  bool inQuotes = false;
  for (std::size_t i = 0; i < value.size(); ++i) {
    const char ch = value[i];
    if (ch == '"') {
      inQuotes = !inQuotes;
      current.push_back(ch);
      continue;
    }
    if (ch == ',' && !inQuotes) {
      std::string item = trimStr(current);
      current.clear();
      if (item.size() < 2 || item.front() != '"' || item.back() != '"') {
        return false;
      }
      outValues->push_back(item.substr(1, item.size() - 2));
      continue;
    }
    current.push_back(ch);
  }
  std::string last = trimStr(current);
  if (last.size() < 2 || last.front() != '"' || last.back() != '"') {
    return false;
  }
  outValues->push_back(last.substr(1, last.size() - 2));
  return true;
}

static bool wildcardMatch(const std::string &pattern, const std::string &text) {
  std::size_t p = 0, t = 0;
  std::size_t star = std::string::npos, match = 0;
  while (t < text.size()) {
    if (p < pattern.size() && (pattern[p] == '?' || pattern[p] == text[t])) {
      ++p;
      ++t;
    } else if (p < pattern.size() && pattern[p] == '*') {
      star = p++;
      match = t;
    } else if (star != std::string::npos) {
      p = star + 1;
      t = ++match;
    } else {
      return false;
    }
  }
  while (p < pattern.size() && pattern[p] == '*') {
    ++p;
  }
  return p == pattern.size();
}

static bool wildcardMatchInsensitive(const std::string &pattern,
                                     const std::string &text) {
  std::string lp = pattern, lt = text;
  std::transform(lp.begin(), lp.end(), lp.begin(), [](unsigned char c) {
    return static_cast<char>(std::tolower(c));
  });
  std::transform(lt.begin(), lt.end(), lt.begin(), [](unsigned char c) {
    return static_cast<char>(std::tolower(c));
  });
  return wildcardMatch(lp, lt);
}

static bool isScriptDocsExcluded(const fs::path &sourcePath,
                                 const NeuronSettings &settings) {
  const std::string fileName = sourcePath.filename().string();
  const std::string stem = sourcePath.stem().string();
  for (const auto &pattern : settings.requireScriptDocsExclude) {
    if (pattern.empty()) {
      continue;
    }
    if (wildcardMatchInsensitive(pattern, fileName) ||
        wildcardMatchInsensitive(pattern, stem)) {
      return true;
    }
  }
  return false;
}

static std::string withAgentHint(const NeuronSettings &settings,
                                 const std::string &message,
                                 const std::string &hint) {
  if (!settings.agentHints || hint.empty()) {
    return message;
  }
  return message + " For agents: " + hint;
}

// ── Flag parsing ────────────────────────────────────────────────────────────

bool parseTraceFlagValue(const std::string &value, bool *outEnabled) {
  if (outEnabled == nullptr) {
    return false;
  }
  std::string normalized = value;
  std::transform(
      normalized.begin(), normalized.end(), normalized.begin(),
      [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  if (normalized == "1" || normalized == "true" || normalized == "yes" ||
      normalized == "on") {
    *outEnabled = true;
    return true;
  }
  if (normalized == "0" || normalized == "false" || normalized == "no" ||
      normalized == "off") {
    *outEnabled = false;
    return true;
  }
  return false;
}

bool isBypassRulesFlag(const std::string &value) {
  return value == "--bypass-rules" || value == "-bypass-rules";
}

void applyBypassRulesToSettings(NeuronSettings *settings) {
  if (settings == nullptr) {
    return;
  }
  settings->maxLinesPerFile = 0;
  settings->maxClassesPerFile = 0;
  settings->requireMethodUppercaseStart = false;
  settings->enforceStrictFileNaming = false;
  settings->forbidRootScripts = false;
  settings->maxLinesPerMethod = 0;
  settings->maxLinesPerBlockStatement = 0;
  settings->minMethodNameLength = 0;
  settings->requireClassExplicitVisibility = false;
  settings->requirePropertyExplicitVisibility = false;
  settings->requireConstUppercase = false;
  settings->maxNestingDepth = 0;
  settings->requireScriptDocs = false;
  settings->requireScriptDocsMinLines = 0;
  settings->maxAutoTestDurationMs = 0;
  settings->requirePublicMethodDocs = false;
  settings->agentHints = false;
}

static bool isStderrTty() {
#ifdef _WIN32
  return _isatty(_fileno(stderr)) != 0;
#else
  return isatty(fileno(stderr)) != 0;
#endif
}

#ifdef _WIN32
static void enableWindowsVirtualTerminal() {
  HANDLE hError = GetStdHandle(STD_ERROR_HANDLE);
  if (hError == INVALID_HANDLE_VALUE || hError == nullptr) {
    return;
  }
  DWORD mode = 0;
  if (!GetConsoleMode(hError, &mode)) {
    return;
  }
  if ((mode & ENABLE_VIRTUAL_TERMINAL_PROCESSING) == 0) {
    SetConsoleMode(hError, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
  }
}
#endif

void initializeTraceErrorsFromEnv() {
  const char *raw = std::getenv("NEURON_TRACE_ERRORS");
  if (raw == nullptr) {
    return;
  }
  bool enabled = false;
  if (parseTraceFlagValue(raw, &enabled)) {
    g_traceErrors = enabled;
  }
}

void initializeDiagnosticColorFromEnv() {
  g_colorDiagnostics = isStderrTty();
  if (std::getenv("NO_COLOR") != nullptr) {
    g_colorDiagnostics = false;
  }
  const char *raw = std::getenv("NEURON_COLOR");
  if (raw != nullptr) {
    bool enabled = false;
    if (parseTraceFlagValue(raw, &enabled)) {
      g_colorDiagnostics = enabled;
    }
  }
#ifdef _WIN32
  if (g_colorDiagnostics) {
    enableWindowsVirtualTerminal();
  }
#endif
}

// ── Argument parsing ────────────────────────────────────────────────────────

std::optional<std::string>
parseFileArgWithTraceFlags(int argc, char *argv[], int startIndex,
                           const std::string &usageLine) {
  std::optional<std::string> fileArg;
  for (int i = startIndex; i < argc; ++i) {
    std::string arg = argv[i];
    if (isBypassRulesFlag(arg)) {
      g_bypassRules = true;
      continue;
    }
    if (arg == "--trace-errors") {
      g_traceErrors = true;
      continue;
    }
    const std::string prefix = "--trace-errors=";
    if (arg.rfind(prefix, 0) == 0) {
      bool enabled = false;
      const std::string value = arg.substr(prefix.size());
      if (!parseTraceFlagValue(value, &enabled)) {
        std::cerr << "Invalid value for --trace-errors: " << value << std::endl;
        return std::nullopt;
      }
      g_traceErrors = enabled;
      continue;
    }
    if (fileArg.has_value()) {
      std::cerr << usageLine << std::endl;
      return std::nullopt;
    }
    fileArg = arg;
  }
  if (!fileArg.has_value()) {
    std::cerr << usageLine << std::endl;
    return std::nullopt;
  }
  return fileArg;
}

// ── Find / load settings file ──────────────────────────────────────────────

std::optional<fs::path> findNearestSettingsFile(const fs::path &startDir) {
  std::error_code ec;
  fs::path dir = fs::weakly_canonical(startDir, ec);
  if (ec) {
    dir = startDir;
  }
  while (!dir.empty()) {
    const fs::path candidate = dir / ".neuronsettings";
    if (fs::exists(candidate)) {
      return candidate;
    }
    const fs::path parent = dir.parent_path();
    if (parent == dir) {
      break;
    }
    dir = parent;
  }
  return std::nullopt;
}

NeuronSettings loadNeuronSettings(const fs::path &pathHint) {
  NeuronSettings settings;

  std::vector<fs::path> searchRoots;
  if (!pathHint.empty()) {
    fs::path hint = pathHint;
    if (hint.has_filename() && hint.extension() == ".nr") {
      hint = hint.parent_path();
    } else if (fs::exists(hint) && fs::is_regular_file(hint)) {
      hint = hint.parent_path();
    }
    if (!hint.empty()) {
      searchRoots.push_back(hint);
    }
  }
  searchRoots.push_back(fs::current_path());

  std::optional<fs::path> settingsPath;
  for (const auto &root : searchRoots) {
    auto candidate = findNearestSettingsFile(root);
    if (candidate.has_value()) {
      settingsPath = candidate;
      break;
    }
  }

  if (!settingsPath.has_value()) {
    if (g_bypassRules) {
      applyBypassRulesToSettings(&settings);
    }
    return settings;
  }

  settings.settingsRoot = settingsPath->parent_path();
  std::ifstream in(*settingsPath);
  if (!in.is_open()) {
    reportConfigWarning(settingsPath->string(), 1,
                        "Failed to open settings file, using defaults.");
    if (g_bypassRules) {
      applyBypassRulesToSettings(&settings);
    }
    return settings;
  }

  std::string line;
  int lineNo = 0;
  while (std::getline(in, line)) {
    lineNo++;
    const std::size_t hashPos = line.find('#');
    if (hashPos != std::string::npos) {
      line = line.substr(0, hashPos);
    }
    line = trimStr(line);
    if (line.empty()) {
      continue;
    }
    if (line.front() == '[' && line.back() == ']') {
      continue;
    }

    const std::size_t eqPos = line.find('=');
    if (eqPos == std::string::npos) {
      reportConfigWarning(settingsPath->string(), lineNo,
                          "Invalid setting line, expected key=value.");
      continue;
    }
    std::string key = trimStr(line.substr(0, eqPos));
    std::string value = trimStr(line.substr(eqPos + 1));
    std::transform(key.begin(), key.end(), key.begin(), [](unsigned char c) {
      return static_cast<char>(std::tolower(c));
    });

    // ── Rule keys ─────────────────────────────────────────────────────────

    if (key == "max_lines_per_file" || key == "rules.max_lines_per_file") {
      int p = 0;
      if (!parsePositiveOrZeroInt(value, &p)) {
        reportConfigWarning(settingsPath->string(), lineNo,
                            "Invalid value for max_lines_per_file.");
        continue;
      }
      settings.maxLinesPerFile = p;
      continue;
    }
    if (key == "max_classes_per_file" || key == "rules.max_classes_per_file") {
      int p = 0;
      if (!parsePositiveOrZeroInt(value, &p)) {
        reportConfigWarning(settingsPath->string(), lineNo,
                            "Invalid value for max_classes_per_file.");
        continue;
      }
      settings.maxClassesPerFile = p;
      continue;
    }
    if (key == "require_method_uppercase_start" ||
        key == "rules.require_method_uppercase_start" ||
        key == "enforce_method_uppercase_start" ||
        key == "rules.enforce_method_uppercase_start") {
      bool p = false;
      if (!parseBoolSetting(value, &p)) {
        reportConfigWarning(
            settingsPath->string(), lineNo,
            "Invalid value for require_method_uppercase_start.");
        continue;
      }
      settings.requireMethodUppercaseStart = p;
      continue;
    }
    if (key == "enforce_strict_file_naming" ||
        key == "rules.enforce_strict_file_naming" ||
        key == "strict_file_naming") {
      bool p = false;
      if (!parseBoolSetting(value, &p)) {
        reportConfigWarning(settingsPath->string(), lineNo,
                            "Invalid value for enforce_strict_file_naming.");
        continue;
      }
      settings.enforceStrictFileNaming = p;
      continue;
    }
    if (key == "forbid_root_scripts" || key == "rules.forbid_root_scripts") {
      bool p = false;
      if (!parseBoolSetting(value, &p)) {
        reportConfigWarning(settingsPath->string(), lineNo,
                            "Invalid value for forbid_root_scripts.");
        continue;
      }
      settings.forbidRootScripts = p;
      continue;
    }
    if (key == "max_lines_per_method" || key == "rules.max_lines_per_method") {
      int p = 0;
      if (!parsePositiveOrZeroInt(value, &p)) {
        reportConfigWarning(settingsPath->string(), lineNo,
                            "Invalid value for max_lines_per_method.");
        continue;
      }
      settings.maxLinesPerMethod = p;
      continue;
    }
    if (key == "max_lines_per_block_statement" ||
        key == "rules.max_lines_per_block_statement") {
      int p = 0;
      if (!parsePositiveOrZeroInt(value, &p)) {
        reportConfigWarning(settingsPath->string(), lineNo,
                            "Invalid value for max_lines_per_block_statement.");
        continue;
      }
      settings.maxLinesPerBlockStatement = p;
      continue;
    }
    if (key == "min_method_name_length" ||
        key == "rules.min_method_name_length") {
      int p = 0;
      if (!parsePositiveOrZeroInt(value, &p)) {
        reportConfigWarning(settingsPath->string(), lineNo,
                            "Invalid value for min_method_name_length.");
        continue;
      }
      settings.minMethodNameLength = p;
      continue;
    }
    if (key == "require_class_explicit_visibility" ||
        key == "rules.require_class_explicit_visibility") {
      bool p = false;
      if (!parseBoolSetting(value, &p)) {
        reportConfigWarning(
            settingsPath->string(), lineNo,
            "Invalid value for require_class_explicit_visibility.");
        continue;
      }
      settings.requireClassExplicitVisibility = p;
      continue;
    }
    if (key == "require_property_explicit_visibility" ||
        key == "rules.require_property_explicit_visibility") {
      bool p = false;
      if (!parseBoolSetting(value, &p)) {
        reportConfigWarning(
            settingsPath->string(), lineNo,
            "Invalid value for require_property_explicit_visibility.");
        continue;
      }
      settings.requirePropertyExplicitVisibility = p;
      continue;
    }
    if (key == "require_const_uppercase" ||
        key == "rules.require_const_uppercase" ||
        key == "require_cost_uppercase" ||
        key == "rules.require_cost_uppercase") {
      bool p = false;
      if (!parseBoolSetting(value, &p)) {
        reportConfigWarning(settingsPath->string(), lineNo,
                            "Invalid value for require_const_uppercase.");
        continue;
      }
      settings.requireConstUppercase = p;
      continue;
    }
    if (key == "max_nesting_depth" || key == "rules.max_nesting_depth") {
      int p = 0;
      if (!parsePositiveOrZeroInt(value, &p)) {
        reportConfigWarning(settingsPath->string(), lineNo,
                            "Invalid value for max_nesting_depth.");
        continue;
      }
      settings.maxNestingDepth = p;
      continue;
    }
    if (key == "require_script_docs" || key == "rules.require_script_docs") {
      bool p = false;
      if (!parseBoolSetting(value, &p)) {
        reportConfigWarning(settingsPath->string(), lineNo,
                            "Invalid value for require_script_docs.");
        continue;
      }
      settings.requireScriptDocs = p;
      continue;
    }
    if (key == "require_script_docs_exclude" ||
        key == "rules.require_script_docs_exclude") {
      std::vector<std::string> p;
      if (!parseStringListSetting(value, &p)) {
        reportConfigWarning(settingsPath->string(), lineNo,
                            "Invalid value for require_script_docs_exclude.");
        continue;
      }
      settings.requireScriptDocsExclude = std::move(p);
      continue;
    }
    if (key == "require_script_docs_min_lines" ||
        key == "rules.require_script_docs_min_lines") {
      int p = 0;
      if (!parsePositiveOrZeroInt(value, &p)) {
        reportConfigWarning(settingsPath->string(), lineNo,
                            "Invalid value for require_script_docs_min_lines.");
        continue;
      }
      settings.requireScriptDocsMinLines = p;
      continue;
    }
    if (key == "max_auto_test_duration_ms" ||
        key == "rules.max_auto_test_duration_ms") {
      int p = 0;
      if (!parsePositiveOrZeroInt(value, &p)) {
        reportConfigWarning(settingsPath->string(), lineNo,
                            "Invalid value for max_auto_test_duration_ms.");
        continue;
      }
      settings.maxAutoTestDurationMs = p;
      continue;
    }
    if (key == "require_public_method_docs" ||
        key == "rules.require_public_method_docs") {
      bool p = false;
      if (!parseBoolSetting(value, &p)) {
        reportConfigWarning(settingsPath->string(), lineNo,
                            "Invalid value for require_public_method_docs.");
        continue;
      }
      settings.requirePublicMethodDocs = p;
      continue;
    }
    if (key == "package_auto_add_missing" ||
        key == "rules.package_auto_add_missing") {
      bool p = false;
      if (!parseBoolSetting(value, &p)) {
        reportConfigWarning(settingsPath->string(), lineNo,
                            "Invalid value for package_auto_add_missing.");
        continue;
      }
      settings.packageAutoAddMissing = p;
      continue;
    }
    if (key == "agent_hints" || key == "rules.agent_hints") {
      bool p = false;
      if (!parseBoolSetting(value, &p)) {
        reportConfigWarning(settingsPath->string(), lineNo,
                            "Invalid value for agent_hints.");
        continue;
      }
      settings.agentHints = p;
      continue;
    }
  }

  if (g_bypassRules) {
    applyBypassRulesToSettings(&settings);
  }
  return settings;
}

// ── Policy verification ──────────────────────────────────────────────────────

bool validateScriptPolicy(const fs::path &sourcePath,
                          const NeuronSettings &settings) {
  if (g_bypassRules) {
    return true;
  }

  const fs::path normalized = sourcePath.lexically_normal();
  std::string extension = normalized.extension().string();
  std::transform(
      extension.begin(), extension.end(), extension.begin(),
      [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  if (extension != ".nr") {
    return true;
  }

  if (settings.forbidRootScripts && !settings.settingsRoot.empty()) {
    std::error_code sourceEc, rootEc;
    const fs::path sourceDir =
        fs::weakly_canonical(normalized.parent_path(), sourceEc);
    const fs::path rootDir =
        fs::weakly_canonical(settings.settingsRoot, rootEc);
    if (!sourceEc && !rootEc && sourceDir == rootDir) {
      std::cerr << "Error: "
                << withAgentHint(settings,
                                 "Scripts cannot live at repository root when "
                                 "forbid_root_scripts = true: '" +
                                     normalized.filename().string() + "'.",
                                 "Move root scripts under folders like src/, "
                                 "modules/, or tests/.")
                << std::endl;
      return false;
    }
  }

  if (!settings.requireScriptDocs ||
      isScriptDocsExcluded(normalized, settings)) {
    return true;
  }

  fs::path docsRoot = settings.settingsRoot.empty() ? normalized.parent_path()
                                                    : settings.settingsRoot;
  const fs::path docsFile =
      docsRoot / "docs" / "scripts" / (normalized.stem().string() + ".md");
  if (!fs::exists(docsFile)) {
    std::cerr << "Error: "
              << withAgentHint(
                     settings,
                     "Missing script documentation: expected '" +
                         docsFile.generic_string() + "'.",
                     "Create docs/scripts/<ScriptName>.md before compiling.")
              << std::endl;
    return false;
  }

  std::ifstream docsIn(docsFile);
  if (!docsIn.is_open()) {
    std::cerr << "Error: Failed to open script documentation file '"
              << docsFile.string() << "'." << std::endl;
    return false;
  }

  int nonEmptyLines = 0;
  std::string line;
  while (std::getline(docsIn, line)) {
    if (!trimStr(line).empty()) {
      ++nonEmptyLines;
    }
  }

  if (settings.requireScriptDocsMinLines > 0 &&
      nonEmptyLines < settings.requireScriptDocsMinLines) {
    std::cerr << "Error: "
              << withAgentHint(
                     settings,
                     "Script documentation '" + docsFile.generic_string() +
                         "' is too short (" + std::to_string(nonEmptyLines) +
                         " non-empty lines, minimum " +
                         std::to_string(settings.requireScriptDocsMinLines) +
                         ").",
                     "Add meaningful usage details to satisfy "
                     "require_script_docs_min_lines.")
              << std::endl;
    return false;
  }

  return true;
}
