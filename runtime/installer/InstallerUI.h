#pragma once

#include <cstdint>
#include <functional>
#include <string>


namespace neuron {
namespace installer {

struct InstallManifest {
  std::string productName;
  std::string productVersion;
  std::string publisher;
  std::string defaultDir;
  bool createDesktopShortcut = true;
  bool createStartMenu = true;
  std::string executableName;
  std::string updaterName;
  std::string uninstallerName;

  // Extracted payload info
  uint64_t payloadBaseOffset = 0;
  uint64_t productOffset = 0;
  uint64_t productSize = 0;
  uint64_t updaterOffset = 0;
  uint64_t updaterSize = 0;
  uint64_t uninstallerOffset = 0;
  uint64_t uninstallerSize = 0;
};

enum class DialogResult {
  Next,
  Back,
  Cancel,
};

class InstallerUI {
public:
  virtual ~InstallerUI() = default;

  virtual bool initialize(const InstallManifest &manifest) = 0;

  virtual DialogResult showWelcome(const InstallManifest &manifest) = 0;
  virtual DialogResult showDirectorySelect(const InstallManifest &manifest,
                                           std::string &installDir) = 0;
  virtual DialogResult
  showProgress(const InstallManifest &manifest, const std::string &installDir,
               std::function<bool(InstallerUI *)> installStep) = 0;
  virtual void setProgressText(const std::string &text) = 0;
  virtual void setProgressPercent(int percent) = 0;
  virtual DialogResult showComplete(const InstallManifest &manifest,
                                    bool &outLaunchApp) = 0;

  virtual void showError(const std::string &title,
                         const std::string &message) = 0;
};

InstallerUI *createInstallerUI();

} // namespace installer
} // namespace neuron
