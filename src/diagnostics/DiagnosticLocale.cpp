#include "neuronc/diagnostics/DiagnosticLocale.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <string>

#ifdef _WIN32
#include <windows.h>
#endif

namespace neuron::diagnostics {

namespace {

std::string trimCopy(std::string text) {
  auto notSpace = [](unsigned char ch) { return !std::isspace(ch); };
  text.erase(text.begin(), std::find_if(text.begin(), text.end(), notSpace));
  text.erase(std::find_if(text.rbegin(), text.rend(), notSpace).base(),
             text.end());
  return text;
}

std::string toLowerCopy(std::string value) {
  std::transform(value.begin(), value.end(), value.begin(),
                 [](unsigned char ch) {
                   return static_cast<char>(std::tolower(ch));
                 });
  return value;
}

std::string canonicalizeCandidate(std::string value) {
  value = trimCopy(std::move(value));
  std::replace(value.begin(), value.end(), '_', '-');
  value = toLowerCopy(std::move(value));
  return value;
}

bool containsLocale(const std::vector<std::string> &supportedLocales,
                    const std::string &locale) {
  return std::find(supportedLocales.begin(), supportedLocales.end(), locale) !=
         supportedLocales.end();
}

std::string fallbackLocale(const std::vector<std::string> &supportedLocales) {
  if (containsLocale(supportedLocales, "en")) {
    return "en";
  }
  if (!supportedLocales.empty()) {
    return supportedLocales.front();
  }
  return "en";
}

std::string matchSupportedLocale(const std::string &normalized,
                                 const std::vector<std::string> &supported) {
  if (normalized.empty()) {
    return "";
  }
  if (containsLocale(supported, normalized)) {
    return normalized;
  }
  const std::size_t dash = normalized.find('-');
  if (dash != std::string::npos) {
    const std::string base = normalized.substr(0, dash);
    if (containsLocale(supported, base)) {
      return base;
    }
  }
  return "";
}

} // namespace

std::vector<std::string> loadSupportedDiagnosticLocales(
    const std::filesystem::path &toolRoot) {
  std::vector<std::string> locales;
  std::ifstream input(toolRoot / "config" / "diagnostics" / "catalog.toml");
  if (!input.is_open()) {
    return locales;
  }

  std::string line;
  while (std::getline(input, line)) {
    line = trimCopy(line);
    if (line.rfind("required_locales", 0) != 0) {
      continue;
    }

    const std::size_t open = line.find('[');
    const std::size_t close = line.rfind(']');
    if (open == std::string::npos || close == std::string::npos ||
        close <= open) {
      return locales;
    }

    const std::string list = line.substr(open + 1, close - open - 1);
    std::string current;
    bool inQuotes = false;
    for (char ch : list) {
      if (ch == '"') {
        inQuotes = !inQuotes;
        if (!inQuotes) {
          const std::string normalized = normalizeLanguageCode(current);
          if (!normalized.empty()) {
            locales.push_back(normalized);
          }
          current.clear();
        }
        continue;
      }
      if (inQuotes) {
        current.push_back(ch);
      }
    }
    break;
  }

  std::sort(locales.begin(), locales.end());
  locales.erase(std::unique(locales.begin(), locales.end()), locales.end());
  return locales;
}

std::string normalizeLanguageCode(const std::string &languageCode) {
  std::string normalized = canonicalizeCandidate(languageCode);
  if (normalized == "c" || normalized == "posix") {
    return "en";
  }
  return normalized;
}

bool isLanguageAutoValue(const std::string &languageCode) {
  return normalizeLanguageCode(languageCode) == "auto";
}

std::optional<std::string> detectOsLanguage() {
#ifdef _WIN32
  wchar_t buffer[LOCALE_NAME_MAX_LENGTH] = {};
  const int result = GetUserDefaultLocaleName(buffer, LOCALE_NAME_MAX_LENGTH);
  if (result <= 0) {
    return std::nullopt;
  }
  std::wstring wide(buffer);
  return normalizeLanguageCode(std::string(wide.begin(), wide.end()));
#else
  return std::nullopt;
#endif
}

ResolvedLanguage resolveLanguagePreference(
    std::string requestedLanguage,
    const std::vector<std::string> &supportedLocales,
    std::optional<std::string> osLanguageOverride) {
  ResolvedLanguage resolved;
  resolved.requested = trimCopy(std::move(requestedLanguage));

  if (isLanguageAutoValue(resolved.requested) || resolved.requested.empty()) {
    resolved.mode = LanguageSettingMode::Auto;
    resolved.normalized = "auto";
    const std::string detected =
        normalizeLanguageCode(osLanguageOverride.value_or(detectOsLanguage().value_or("")));
    const std::string matched = matchSupportedLocale(detected, supportedLocales);
    if (!matched.empty()) {
      resolved.effective = matched;
      resolved.source = ResolvedLanguageSource::AutoOs;
      return resolved;
    }
    resolved.effective = fallbackLocale(supportedLocales);
    resolved.source = ResolvedLanguageSource::FallbackEnglish;
    return resolved;
  }

  resolved.mode = LanguageSettingMode::Explicit;
  resolved.normalized = normalizeLanguageCode(resolved.requested);
  const std::string matched =
      matchSupportedLocale(resolved.normalized, supportedLocales);
  if (!matched.empty()) {
    resolved.effective = matched;
    resolved.source = ResolvedLanguageSource::Explicit;
    return resolved;
  }

  resolved.effective = fallbackLocale(supportedLocales);
  resolved.source = ResolvedLanguageSource::FallbackEnglish;
  return resolved;
}

const char *resolvedLanguageSourceName(ResolvedLanguageSource source) {
  switch (source) {
  case ResolvedLanguageSource::Explicit:
    return "explicit";
  case ResolvedLanguageSource::AutoOs:
    return "auto(os)";
  case ResolvedLanguageSource::FallbackEnglish:
    return "fallback(en)";
  }
  return "fallback(en)";
}

} // namespace neuron::diagnostics

