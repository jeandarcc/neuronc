#pragma once

#include <string>

namespace neuron::installer {

struct UpdatePromptInfo {
  std::string currentVersion;
  std::string latestVersion;
  std::string notes;
};

class UpdaterUI {
public:
  virtual ~UpdaterUI() = default;

  virtual bool confirmUpdate(const UpdatePromptInfo &info) = 0;
  virtual void setStatus(const std::string &statusText) = 0;
  virtual void setProgress(int percent) = 0;
  virtual void showError(const std::string &title,
                         const std::string &message) = 0;
  virtual void showInfo(const std::string &title,
                        const std::string &message) = 0;
};

UpdaterUI *createUpdaterUI();

} // namespace neuron::installer

