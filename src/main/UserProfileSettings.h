#pragma once

#include <filesystem>
#include <optional>
#include <string>

namespace neuron {

struct UserProfileSettings {
  std::string language = "auto";
};

std::filesystem::path userProfileSettingsPath();

std::optional<UserProfileSettings> loadUserProfileSettings();

bool saveUserProfileSettings(const UserProfileSettings &settings);

} // namespace neuron

