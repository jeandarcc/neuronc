#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace neuron::diagnostics {

enum class LanguageSettingMode {
  Auto,
  Explicit,
};

enum class ResolvedLanguageSource {
  Explicit,
  AutoOs,
  FallbackEnglish,
};

struct ResolvedLanguage {
  std::string requested;
  std::string normalized;
  std::string effective;
  LanguageSettingMode mode = LanguageSettingMode::Auto;
  ResolvedLanguageSource source = ResolvedLanguageSource::FallbackEnglish;
};

std::vector<std::string> loadSupportedDiagnosticLocales(
    const std::filesystem::path &toolRoot);

std::string normalizeLanguageCode(const std::string &languageCode);

bool isLanguageAutoValue(const std::string &languageCode);

std::optional<std::string> detectOsLanguage();

ResolvedLanguage resolveLanguagePreference(
    std::string requestedLanguage,
    const std::vector<std::string> &supportedLocales,
    std::optional<std::string> osLanguageOverride = std::nullopt);

const char *resolvedLanguageSourceName(ResolvedLanguageSource source);

} // namespace neuron::diagnostics

