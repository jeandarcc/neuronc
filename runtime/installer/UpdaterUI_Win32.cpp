#include "UpdaterUI.h"

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

#include <iostream>
#include <sstream>

namespace neuron::installer {

namespace {

#ifdef _WIN32
class Win32UpdaterUI final : public UpdaterUI {
public:
  bool confirmUpdate(const UpdatePromptInfo &info) override {
    std::ostringstream message;
    message << "A new update is available.\n\n";
    message << "Current: " << info.currentVersion << "\n";
    message << "Latest:  " << info.latestVersion << "\n";
    if (!info.notes.empty()) {
      message << "\nRelease Notes:\n" << info.notes << "\n";
    }
    message << "\nInstall update now?";

    const int result = MessageBoxA(nullptr, message.str().c_str(),
                                   "Neuron Updater",
                                   MB_YESNO | MB_ICONQUESTION | MB_TOPMOST);
    return result == IDYES;
  }

  void setStatus(const std::string &statusText) override {
    std::cout << "[updater] " << statusText << std::endl;
  }

  void setProgress(int percent) override {
    if (percent < 0) {
      percent = 0;
    }
    if (percent > 100) {
      percent = 100;
    }
    std::cout << "[updater] progress: " << percent << "%" << std::endl;
  }

  void showError(const std::string &title,
                 const std::string &message) override {
    MessageBoxA(nullptr, message.c_str(), title.c_str(),
                MB_OK | MB_ICONERROR | MB_TOPMOST);
  }

  void showInfo(const std::string &title,
                const std::string &message) override {
    MessageBoxA(nullptr, message.c_str(), title.c_str(),
                MB_OK | MB_ICONINFORMATION | MB_TOPMOST);
  }
};
#else
class StubUpdaterUI final : public UpdaterUI {
public:
  bool confirmUpdate(const UpdatePromptInfo &) override { return true; }
  void setStatus(const std::string &statusText) override {
    std::cout << "[updater] " << statusText << std::endl;
  }
  void setProgress(int percent) override {
    std::cout << "[updater] progress: " << percent << "%" << std::endl;
  }
  void showError(const std::string &title,
                 const std::string &message) override {
    std::cerr << title << ": " << message << std::endl;
  }
  void showInfo(const std::string &title,
                const std::string &message) override {
    std::cout << title << ": " << message << std::endl;
  }
};
#endif

} // namespace

UpdaterUI *createUpdaterUI() {
#ifdef _WIN32
  return new Win32UpdaterUI();
#else
  return new StubUpdaterUI();
#endif
}

} // namespace neuron::installer

