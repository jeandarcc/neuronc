#include "neuronc/cli/ProductSettings.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>

namespace neuron {

namespace {

std::string trim(const std::string &s) {
  auto start = s.find_first_not_of(" \t\r\n");
  if (start == std::string::npos)
    return "";
  auto end = s.find_last_not_of(" \t\r\n");
  return s.substr(start, end - start + 1);
}

std::string unquote(const std::string &s) {
  if (s.size() >= 2 && s.front() == '"' && s.back() == '"') {
    return s.substr(1, s.size() - 2);
  }
  return s;
}

bool parseBool(const std::string &value, bool *out) {
  std::string lower = value;
  std::transform(
      lower.begin(), lower.end(), lower.begin(),
      [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  if (lower == "true" || lower == "1" || lower == "yes") {
    *out = true;
    return true;
  }
  if (lower == "false" || lower == "0" || lower == "no") {
    *out = false;
    return true;
  }
  return false;
}

bool parseInt(const std::string &value, int *out) {
  try {
    *out = std::stoi(value);
    return true;
  } catch (...) {
    return false;
  }
}

std::vector<std::string> parseStringArray(const std::string &value) {
  std::vector<std::string> result;
  // Expected format: ["ext1", "ext2"]
  std::string inner = value;
  auto lb = inner.find('[');
  auto rb = inner.rfind(']');
  if (lb != std::string::npos && rb != std::string::npos && rb > lb) {
    inner = inner.substr(lb + 1, rb - lb - 1);
  }
  std::istringstream stream(inner);
  std::string token;
  while (std::getline(stream, token, ',')) {
    std::string trimmed = trim(token);
    if (!trimmed.empty()) {
      result.push_back(unquote(trimmed));
    }
  }
  return result;
}

} // namespace

bool parseProductSettings(const std::filesystem::path &path,
                          ProductSettings *out,
                          std::vector<ProductSettingsError> *errors) {
  if (out == nullptr) {
    return false;
  }

  std::ifstream file(path);
  if (!file.is_open()) {
    if (errors) {
      errors->push_back({0, "Cannot open .productsettings: " + path.string()});
    }
    return false;
  }

  ProductSettings settings;
  std::string line;
  int lineNo = 0;
  bool success = true;

  while (std::getline(file, line)) {
    ++lineNo;
    std::string trimmed = trim(line);
    if (trimmed.empty() || trimmed[0] == '#') {
      continue;
    }

    auto eq = trimmed.find('=');
    if (eq == std::string::npos) {
      if (errors) {
        errors->push_back({lineNo, "Invalid line (missing '='): " + trimmed});
      }
      success = false;
      continue;
    }

    std::string key = trim(trimmed.substr(0, eq));
    std::string value = trim(trimmed.substr(eq + 1));
    std::string strValue = unquote(value);

    // â”€â”€ Identity â”€â”€
    if (key == "product_name") {
      settings.productName = strValue;
    } else if (key == "product_version") {
      settings.productVersion = strValue;
    } else if (key == "product_build_version") {
      if (!parseInt(strValue, &settings.productBuildVersion)) {
        if (errors) {
          errors->push_back(
              {lineNo, "Invalid int for product_build_version: " + value});
        }
        success = false;
      }
    } else if (key == "product_publisher") {
      settings.productPublisher = strValue;
    } else if (key == "product_description") {
      settings.productDescription = strValue;
    } else if (key == "product_website") {
      settings.productWebsite = strValue;
    }
    // â”€â”€ Branding â”€â”€
    else if (key == "icon_windows") {
      settings.iconWindows = strValue;
    } else if (key == "icon_linux") {
      settings.iconLinux = strValue;
    } else if (key == "icon_macos") {
      settings.iconMacos = strValue;
    } else if (key == "splash_image") {
      settings.splashImage = strValue;
    }
    // â”€â”€ Output â”€â”€
    else if (key == "output_name") {
      settings.outputName = strValue;
    } else if (key == "output_dir") {
      settings.outputDir = strValue;
    }
    // â”€â”€ Installer â”€â”€
    else if (key == "installer_enabled") {
      if (!parseBool(strValue, &settings.installerEnabled)) {
        if (errors) {
          errors->push_back(
              {lineNo, "Invalid bool for installer_enabled: " + value});
        }
        success = false;
      }
    } else if (key == "installer_style") {
      settings.installerStyle = strValue;
    } else if (key == "installer_license_file") {
      settings.installerLicenseFile = strValue;
    } else if (key == "installer_banner_image") {
      settings.installerBannerImage = strValue;
    } else if (key == "installer_accent_color") {
      settings.installerAccentColor = strValue;
    } else if (key == "install_directory_default") {
      settings.installDirectoryDefault = strValue;
    } else if (key == "create_desktop_shortcut") {
      if (!parseBool(strValue, &settings.createDesktopShortcut)) {
        if (errors) {
          errors->push_back(
              {lineNo, "Invalid bool for create_desktop_shortcut: " + value});
        }
        success = false;
      }
    } else if (key == "create_start_menu_entry") {
      if (!parseBool(strValue, &settings.createStartMenuEntry)) {
        if (errors) {
          errors->push_back(
              {lineNo, "Invalid bool for create_start_menu_entry: " + value});
        }
        success = false;
      }
    } else if (key == "file_associations") {
      settings.fileAssociations = parseStringArray(value);
    }
    // â”€â”€ Update System â”€â”€
    else if (key == "update_enabled") {
      if (!parseBool(strValue, &settings.updateEnabled)) {
        if (errors) {
          errors->push_back(
              {lineNo, "Invalid bool for update_enabled: " + value});
        }
        success = false;
      }
    } else if (key == "update_url") {
      settings.updateUrl = strValue;
    } else if (key == "update_check_interval_hours") {
      if (!parseInt(strValue, &settings.updateCheckIntervalHours)) {
        if (errors) {
          errors->push_back(
              {lineNo,
               "Invalid int for update_check_interval_hours: " + value});
        }
        success = false;
      }
    } else if (key == "update_channel") {
      settings.updateChannel = strValue;
    } else if (key == "update_public_key") {
      settings.updatePublicKey = strValue;
    }
    // â”€â”€ Uninstaller â”€â”€
    else if (key == "uninstaller_enabled") {
      if (!parseBool(strValue, &settings.uninstallerEnabled)) {
        if (errors) {
          errors->push_back(
              {lineNo, "Invalid bool for uninstaller_enabled: " + value});
        }
        success = false;
      }
    } else if (key == "uninstaller_name") {
      settings.uninstallerName = strValue;
    }
    // â”€â”€ Unknown â”€â”€
    else {
      if (errors) {
        errors->push_back({lineNo, "Unknown setting: " + key});
      }
    }
  }

  // Apply defaults for derived fields
  if (settings.installDirectoryDefault.empty()) {
    settings.installDirectoryDefault =
        "%PROGRAMFILES%\\" + settings.productName;
  }
  if (settings.uninstallerName.empty()) {
    settings.uninstallerName = "Uninstall " + settings.productName;
  }

  *out = std::move(settings);
  return success;
}

bool saveProductSettings(const std::filesystem::path &path,
                         const ProductSettings &settings,
                         const std::string &originalContent) {
  std::istringstream in(originalContent);
  std::ostringstream out;
  std::string line;

  bool foundProductVersion = false;
  bool foundBuildVersion = false;

  while (std::getline(in, line)) {
    std::string trimmed = trim(line);
    if (!trimmed.empty() && trimmed[0] != '#') {
      auto eq = trimmed.find('=');
      if (eq != std::string::npos) {
        std::string key = trim(trimmed.substr(0, eq));
        if (key == "product_version") {
          out << "product_version = \"" << settings.productVersion << "\"\n";
          foundProductVersion = true;
          continue;
        }
        if (key == "product_build_version") {
          out << "product_build_version = " << settings.productBuildVersion
              << "\n";
          foundBuildVersion = true;
          continue;
        }
      }
    }
    out << line << "\n";
  }

  // Append missing identity keys if they were absent.
  if (!foundProductVersion) {
    out << "product_version = \"" << settings.productVersion << "\"\n";
  }
  if (!foundBuildVersion) {
    out << "product_build_version = " << settings.productBuildVersion << "\n";
  }

  std::ofstream file(path);
  if (!file.is_open()) {
    return false;
  }
  file << out.str();
  return true;
}

std::string generateDefaultProductSettings(const std::string &projectName) {
  std::ostringstream ss;
  ss << "# Product build settings for " << projectName << "\n"
     << "\n"
     << "# â”€â”€ Identity â”€â”€\n"
     << "product_name = \"" << projectName << "\"\n"
     << "product_version = \"1.0.0\"\n"
     << "product_build_version = 1\n"
     << "product_publisher = \"\"\n"
     << "product_description = \"A Neuron application\"\n"
     << "product_website = \"\"\n"
     << "\n"
     << "# â”€â”€ Branding â”€â”€\n"
     << "icon_windows = \"assets/icon.ico\"\n"
     << "icon_linux = \"assets/icon.png\"\n"
     << "icon_macos = \"assets/icon.icns\"\n"
     << "splash_image = \"\"\n"
     << "\n"
     << "# â”€â”€ Output â”€â”€\n"
     << "output_name = \"" << projectName << "\"\n"
     << "output_dir = \"build/product\"\n"
     << "\n"
     << "# â”€â”€ Installer â”€â”€\n"
     << "installer_enabled = true\n"
     << "installer_style = \"modern\"\n"
     << "installer_license_file = \"\"\n"
     << "installer_banner_image = \"\"\n"
     << "installer_accent_color = \"#0078D4\"\n"
     << "install_directory_default = \"%PROGRAMFILES%\\\\" << projectName
     << "\"\n"
     << "create_desktop_shortcut = true\n"
     << "create_start_menu_entry = true\n"
     << "file_associations = []\n"
     << "\n"
     << "# â”€â”€ Update System â”€â”€\n"
     << "update_enabled = false\n"
     << "update_url = \"\"\n"
     << "update_check_interval_hours = 24\n"
     << "update_channel = \"stable\"\n"
     << "update_public_key = \"\"\n"
     << "\n"
     << "# â”€â”€ Uninstaller â”€â”€\n"
     << "uninstaller_enabled = true\n"
     << "uninstaller_name = \"Uninstall " << projectName << "\"\n";
  return ss.str();
}

} // namespace neuron
