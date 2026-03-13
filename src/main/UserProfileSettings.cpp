#include "UserProfileSettings.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <fstream>
#include <system_error>

namespace neuron {

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

std::filesystem::path resolveUserConfigRoot() {
#ifdef _WIN32
  if (const char *appData = std::getenv("APPDATA")) {
    if (*appData != '\0') {
      return std::filesystem::path(appData) / "Neuron";
    }
  }
#endif
  if (const char *home = std::getenv("USERPROFILE")) {
    if (*home != '\0') {
      return std::filesystem::path(home) / ".Neuron";
    }
  }
  return std::filesystem::temp_directory_path() / "Neuron";
}

} // namespace

std::filesystem::path userProfileSettingsPath() {
  return resolveUserConfigRoot() / "settings.toml";
}

std::optional<UserProfileSettings> loadUserProfileSettings() {
  UserProfileSettings settings;
  const std::filesystem::path path = userProfileSettingsPath();
  std::ifstream input(path);
  if (!input.is_open()) {
    return settings;
  }

  std::string line;
  while (std::getline(input, line)) {
    const std::size_t hash = line.find('#');
    if (hash != std::string::npos) {
      line = line.substr(0, hash);
    }
    line = trimCopy(line);
    if (line.empty() || (line.front() == '[' && line.back() == ']')) {
      continue;
    }

    const std::size_t eq = line.find('=');
    if (eq == std::string::npos) {
      continue;
    }

    std::string key = toLowerCopy(trimCopy(line.substr(0, eq)));
    std::string value = trimCopy(line.substr(eq + 1));
    if (value.size() >= 2 && value.front() == '"' && value.back() == '"') {
      value = value.substr(1, value.size() - 2);
    }

    if (key == "language") {
      settings.language = trimCopy(value);
    }
  }

  return settings;
}

bool saveUserProfileSettings(const UserProfileSettings &settings) {
  const std::filesystem::path path = userProfileSettingsPath();
  std::error_code ec;
  std::filesystem::create_directories(path.parent_path(), ec);
  if (ec) {
    return false;
  }

  std::ofstream output(path, std::ios::trunc);
  if (!output.is_open()) {
    return false;
  }

  output << "language = \"" << settings.language << "\"\n";
  return output.good();
}

} // namespace neuron

