#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <shellapi.h>
#endif

namespace fs = std::filesystem;

namespace neuron::installer {

namespace {

std::string getExecutablePath() {
#ifdef _WIN32
  char path[MAX_PATH];
  const DWORD len = GetModuleFileNameA(nullptr, path, MAX_PATH);
  if (len > 0 && len < MAX_PATH) {
    return std::string(path, len);
  }
#endif
  return {};
}

bool isSafeRemovalTarget(const fs::path &target) {
  if (target.empty()) {
    return false;
  }
  const fs::path absolute = fs::absolute(target).lexically_normal();
  if (absolute == absolute.root_path()) {
    return false;
  }
  const std::string asString = absolute.string();
  if (asString.size() < 4u) {
    return false;
  }
  return true;
}

int showConfirmDialog(const std::string &targetPath) {
#ifdef _WIN32
  std::string message =
      "This will uninstall the application and remove all files under:\n\n" +
      targetPath + "\n\nContinue?";
  return MessageBoxA(nullptr, message.c_str(), "Neuron Uninstaller",
                     MB_YESNO | MB_ICONQUESTION | MB_TOPMOST);
#else
  (void)targetPath;
  return 0;
#endif
}

bool scheduleRemovalScript(const fs::path &targetPath, std::string *outError) {
#ifdef _WIN32
  char tempPathBuffer[MAX_PATH];
  const DWORD tempLen = GetTempPathA(MAX_PATH, tempPathBuffer);
  if (tempLen == 0 || tempLen >= MAX_PATH) {
    if (outError != nullptr) {
      *outError = "failed to locate temporary directory";
    }
    return false;
  }

  const fs::path scriptPath =
      fs::path(tempPathBuffer) /
      ("npp_uninstall_" + std::to_string(GetCurrentProcessId()) + ".cmd");
  const fs::path normalizedTarget = fs::absolute(targetPath).lexically_normal();

  std::ofstream script(scriptPath, std::ios::trunc);
  if (!script.is_open()) {
    if (outError != nullptr) {
      *outError = "failed to create uninstall script";
    }
    return false;
  }

  script << "@echo off\n";
  script << "set TARGET=\"" << normalizedTarget.string() << "\"\n";
  script << "timeout /t 2 /nobreak >nul\n";
  script << "rmdir /s /q %TARGET%\n";
  script << "del \"%~f0\"\n";
  script.close();

  if (!script.good()) {
    if (outError != nullptr) {
      *outError = "failed to write uninstall script";
    }
    return false;
  }

  const std::string params = "/C \"" + scriptPath.string() + "\"";
  HINSTANCE hInst =
      ShellExecuteA(nullptr, "open", "cmd.exe", params.c_str(), nullptr, SW_HIDE);
  if (reinterpret_cast<intptr_t>(hInst) <= 32) {
    if (outError != nullptr) {
      *outError = "failed to launch uninstall script";
    }
    return false;
  }
  return true;
#else
  (void)targetPath;
  if (outError != nullptr) {
    *outError = "uninstaller is only supported on Windows";
  }
  return false;
#endif
}

} // namespace

int runUninstaller(int argc, char **argv) {
  fs::path targetDir = fs::path(getExecutablePath()).parent_path();
  bool quiet = false;

  for (int i = 1; i < argc; ++i) {
    const std::string arg = argv[i];
    if (arg == "--target" && i + 1 < argc) {
      targetDir = fs::path(argv[++i]);
      continue;
    }
    if (arg == "--quiet") {
      quiet = true;
      continue;
    }
  }

  if (!isSafeRemovalTarget(targetDir)) {
#ifdef _WIN32
    MessageBoxA(nullptr, "Unsafe uninstall target path.",
                "Neuron Uninstaller", MB_OK | MB_ICONERROR | MB_TOPMOST);
#endif
    return 1;
  }

  if (!quiet) {
    const int confirm = showConfirmDialog(targetDir.string());
#ifdef _WIN32
    if (confirm != IDYES) {
      return 1;
    }
#endif
  }

  std::string error;
  if (!scheduleRemovalScript(targetDir, &error)) {
#ifdef _WIN32
    MessageBoxA(nullptr, error.c_str(), "Neuron Uninstaller",
                MB_OK | MB_ICONERROR | MB_TOPMOST);
#endif
    return 1;
  }

#ifdef _WIN32
  MessageBoxA(nullptr, "Uninstall has started. Application files will be removed.",
              "Neuron Uninstaller",
              MB_OK | MB_ICONINFORMATION | MB_TOPMOST);
#endif
  return 0;
}

} // namespace neuron::installer

#ifdef _WIN32
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {
  return neuron::installer::runUninstaller(__argc, __argv);
}
#else
int main(int argc, char **argv) {
  return neuron::installer::runUninstaller(argc, argv);
}
#endif
