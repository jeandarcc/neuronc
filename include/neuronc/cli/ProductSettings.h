#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace neuron {

struct ProductSettings {
  // â”€â”€ Identity â”€â”€
  std::string productName = "My Application";
  std::string productVersion = "1.0.0";
  int productBuildVersion = 1;
  std::string productPublisher;
  std::string productDescription = "A Neuron application";
  std::string productWebsite;

  // â”€â”€ Branding â”€â”€
  std::filesystem::path iconWindows = "assets/icon.ico";
  std::filesystem::path iconLinux = "assets/icon.png";
  std::filesystem::path iconMacos = "assets/icon.icns";
  std::filesystem::path splashImage;

  // â”€â”€ Output â”€â”€
  std::string outputName = "MyApp";
  std::filesystem::path outputDir = "build/product";

  // â”€â”€ Installer â”€â”€
  bool installerEnabled = true;
  std::string installerStyle = "modern";
  std::filesystem::path installerLicenseFile;
  std::filesystem::path installerBannerImage;
  std::string installerAccentColor = "#0078D4";
  std::string installDirectoryDefault;
  bool createDesktopShortcut = true;
  bool createStartMenuEntry = true;
  std::vector<std::string> fileAssociations;

  // â”€â”€ Update System â”€â”€
  bool updateEnabled = false;
  std::string updateUrl;
  int updateCheckIntervalHours = 24;
  std::string updateChannel = "stable";
  std::string updatePublicKey;

  // â”€â”€ Uninstaller â”€â”€
  bool uninstallerEnabled = true;
  std::string uninstallerName;
};

struct ProductSettingsError {
  int line = 0;
  std::string message;
};

/// Parse a .productsettings file.  Returns true on success.
bool parseProductSettings(const std::filesystem::path &path,
                          ProductSettings *out,
                          std::vector<ProductSettingsError> *errors);

/// Save ProductSettings back to a .productsettings file (used for
/// auto-increment).
bool saveProductSettings(const std::filesystem::path &path,
                         const ProductSettings &settings,
                         const std::string &originalContent);

/// Generate default .productsettings content for a new project.
std::string generateDefaultProductSettings(const std::string &projectName);

} // namespace neuron
