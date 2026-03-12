#include "InstallerUI.h"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <vector>


#ifdef _WIN32
#include <windows.h>
// Include the Win32 implementation directly for unity build
#include "InstallerUI_Win32.cpp"
#endif

namespace fs = std::filesystem;

namespace neuron {
namespace installer {

static constexpr char kEmbedMagic[8] = {'N', 'P', 'P', 'I', 'N', 'S', 'T', '1'};
static constexpr size_t kFooterSize =
    24; // 8b manifest size + 8b payload blob size + 8b magic

std::string getExecutablePath() {
#ifdef _WIN32
  char path[MAX_PATH];
  GetModuleFileNameA(NULL, path, MAX_PATH);
  return std::string(path);
#else
  return "";
#endif
}

// Very basic JSON extractor since we don't want to link a heavy library
std::string extractJsonField(const std::string &json, const std::string &key) {
  std::string search = "\"" + key + "\":";
  size_t pos = json.find(search);
  if (pos == std::string::npos)
    return "";

  pos = json.find("\"", pos + search.length());
  if (pos == std::string::npos) {
    // Maybe it's a boolean?
    size_t bpos = json.find_first_not_of(" \t", pos + search.length() - 1);
    if (bpos != std::string::npos) {
      if (json.substr(bpos, 4) == "true")
        return "true";
      if (json.substr(bpos, 5) == "false")
        return "false";
    }
    return "";
  }

  size_t endPos = json.find("\"", pos + 1);
  if (endPos == std::string::npos)
    return "";

  return json.substr(pos + 1, endPos - pos - 1);
}

uint64_t extractJsonUIntField(const std::string &json, const std::string &key) {
  const std::string search = "\"" + key + "\":";
  const size_t pos = json.find(search);
  if (pos == std::string::npos) {
    return 0;
  }

  const size_t valueStart = json.find_first_of("0123456789", pos + search.size());
  if (valueStart == std::string::npos) {
    return 0;
  }
  size_t valueEnd = valueStart;
  while (valueEnd < json.size() &&
         std::isdigit(static_cast<unsigned char>(json[valueEnd]))) {
    valueEnd++;
  }
  const std::string token = json.substr(valueStart, valueEnd - valueStart);
  if (token.empty()) {
    return 0;
  }
  try {
    return static_cast<uint64_t>(std::stoull(token));
  } catch (...) {
    return 0;
  }
}

bool readManifest(InstallManifest &outManifest) {
  std::string exePath = getExecutablePath();
  std::ifstream file(exePath, std::ios::binary | std::ios::ate);
  if (!file.is_open())
    return false;

  auto fileSize = static_cast<uint64_t>(file.tellg());
  if (fileSize < kFooterSize)
    return false;

  file.seekg(-static_cast<std::streamoff>(kFooterSize), std::ios::end);

  uint64_t manifestSize = 0;
  uint64_t payloadBlobSize = 0;
  char magic[8];

  file.read(reinterpret_cast<char *>(&manifestSize), 8);
  file.read(reinterpret_cast<char *>(&payloadBlobSize), 8);
  file.read(magic, 8);

  if (std::memcmp(magic, kEmbedMagic, 8) != 0)
    return false;

  const uint64_t totalPayloadSize = manifestSize + payloadBlobSize;
  if (totalPayloadSize + kFooterSize > fileSize) {
    return false;
  }

  file.seekg(-static_cast<std::streamoff>(totalPayloadSize + kFooterSize),
             std::ios::end);
  outManifest.payloadBaseOffset = static_cast<uint64_t>(file.tellg());

  // Read JSON manifest
  file.seekg(outManifest.payloadBaseOffset + payloadBlobSize, std::ios::beg);
  std::vector<char> jsonBuf(manifestSize + 1, 0);
  file.read(jsonBuf.data(), manifestSize);

  std::string json(jsonBuf.data());
  outManifest.productName = extractJsonField(json, "product_name");
  outManifest.productVersion = extractJsonField(json, "product_version");
  outManifest.publisher = extractJsonField(json, "publisher");
  outManifest.defaultDir = extractJsonField(json, "default_dir");
  outManifest.executableName = extractJsonField(json, "executable_name");
  outManifest.updaterName = extractJsonField(json, "updater_executable");
  outManifest.uninstallerName = extractJsonField(json, "uninstaller_executable");
  outManifest.productOffset = extractJsonUIntField(json, "product_offset");
  outManifest.productSize = extractJsonUIntField(json, "product_size");
  outManifest.updaterOffset = extractJsonUIntField(json, "updater_offset");
  outManifest.updaterSize = extractJsonUIntField(json, "updater_size");
  outManifest.uninstallerOffset = extractJsonUIntField(json, "uninstaller_offset");
  outManifest.uninstallerSize = extractJsonUIntField(json, "uninstaller_size");

  if (outManifest.productSize == 0) {
    // Backward compatibility with older payloads that only embed the product exe.
    outManifest.productOffset = 0;
    outManifest.productSize = payloadBlobSize;
  }

  auto sanitizePayload = [payloadBlobSize](uint64_t *offset, uint64_t *size) {
    if (offset == nullptr || size == nullptr) {
      return;
    }
    if (*size == 0) {
      *offset = 0;
      return;
    }
    if (*offset > payloadBlobSize || *offset + *size > payloadBlobSize) {
      *offset = 0;
      *size = 0;
    }
  };
  sanitizePayload(&outManifest.productOffset, &outManifest.productSize);
  sanitizePayload(&outManifest.updaterOffset, &outManifest.updaterSize);
  sanitizePayload(&outManifest.uninstallerOffset, &outManifest.uninstallerSize);

  // Simple expansion for %PROGRAMFILES%
  if (outManifest.defaultDir.find("%PROGRAMFILES%") != std::string::npos) {
#ifdef _WIN32
    const char *pf = getenv("PROGRAMW6432");
    if (!pf)
      pf = getenv("PROGRAMFILES");
    std::string pfStr = pf ? pf : "C:\\Program Files";

    size_t pct = outManifest.defaultDir.find("%PROGRAMFILES%");
    outManifest.defaultDir.replace(pct, 14, pfStr);
#endif
  }

  if (outManifest.defaultDir.find("{product_name}") != std::string::npos) {
    size_t pct = outManifest.defaultDir.find("{product_name}");
    outManifest.defaultDir.replace(pct, 14, outManifest.productName);
  }

  return true;
}

bool performExtraction(InstallerUI *ui, const InstallManifest &manifest,
                       const std::string &targetDir) {
  try {
    fs::create_directories(targetDir);
  } catch (...) {
    ui->showError("Error",
                  "Could not create installation directory:\n" + targetDir);
    return false;
  }

  ui->setProgressText("Extracting payloads...");
  ui->setProgressPercent(10);

  std::ifstream inFile(getExecutablePath(), std::ios::binary);
  if (!inFile.is_open()) {
    ui->showError("Error", "Could not read installer binary.");
    return false;
  }

  const size_t bufSize = 1024 * 1024; // 1MB buffer
  std::vector<char> buffer(bufSize);
  uint64_t totalPayloadBytes = manifest.productSize;
  if (!manifest.updaterName.empty()) {
    totalPayloadBytes += manifest.updaterSize;
  }
  if (!manifest.uninstallerName.empty()) {
    totalPayloadBytes += manifest.uninstallerSize;
  }
  if (totalPayloadBytes == 0) {
    ui->showError("Error", "Embedded payload is empty.");
    return false;
  }

  uint64_t processedBytes = 0;
  auto extractPayload = [&](const std::string &label, const fs::path &targetPath,
                            uint64_t payloadOffset,
                            uint64_t payloadSize) -> bool {
    if (payloadSize == 0) {
      return true;
    }

    std::ofstream outFile(targetPath, std::ios::binary | std::ios::trunc);
    if (!outFile.is_open()) {
      ui->showError("Error",
                    "Could not write payload to:\n" + targetPath.string());
      return false;
    }

    ui->setProgressText("Extracting " + label + "...");
    inFile.seekg(static_cast<std::streamoff>(manifest.payloadBaseOffset +
                                             payloadOffset),
                 std::ios::beg);
    uint64_t remaining = payloadSize;
    while (remaining > 0) {
      const size_t toRead =
          static_cast<size_t>(std::min<uint64_t>(remaining, bufSize));
      inFile.read(buffer.data(), static_cast<std::streamsize>(toRead));
      if (!inFile.good()) {
        ui->showError("Error", "Failed while reading embedded payload data.");
        return false;
      }
      outFile.write(buffer.data(), static_cast<std::streamsize>(toRead));
      if (!outFile.good()) {
        ui->showError("Error", "Failed while writing extracted payload.");
        return false;
      }
      remaining -= toRead;
      processedBytes += toRead;
      const int percent =
          10 + static_cast<int>(90.0 * static_cast<double>(processedBytes) /
                                static_cast<double>(totalPayloadBytes));
      ui->setProgressPercent(std::min(percent, 100));
    }
    return true;
  };

  if (!extractPayload(manifest.executableName,
                      fs::path(targetDir) / manifest.executableName,
                      manifest.productOffset, manifest.productSize)) {
    return false;
  }
  if (!manifest.updaterName.empty() && manifest.updaterSize > 0) {
    if (!extractPayload(manifest.updaterName,
                        fs::path(targetDir) / manifest.updaterName,
                        manifest.updaterOffset, manifest.updaterSize)) {
      return false;
    }
  }
  if (!manifest.uninstallerName.empty() && manifest.uninstallerSize > 0) {
    if (!extractPayload(manifest.uninstallerName,
                        fs::path(targetDir) / manifest.uninstallerName,
                        manifest.uninstallerOffset, manifest.uninstallerSize)) {
      return false;
    }
  }

  // TODO: Create Desktop Shortcut, Start Menu
  // TODO: Write Uninstall Key to Registry

  ui->setProgressPercent(100);
  ui->setProgressText("Installation complete.");
  return true;
}

int runInstaller() {
  InstallManifest manifest;
  if (!readManifest(manifest)) {
    // Fallback or dev mode
    manifest.productName = "Unknown Product";
    manifest.productVersion = "1.0.0";
    manifest.defaultDir = "C:\\Program Files\\Unknown";
    manifest.executableName = "Unknown.exe";
  }

  InstallerUI *ui = createInstallerUI();
  if (!ui)
    return 1;

  if (!ui->initialize(manifest)) {
    delete ui;
    return 1;
  }

  std::string currentDir = manifest.defaultDir;
  int currentStep = 0; // 0: Welcome, 1: Directory, 2: Progress, 3: Complete
  bool isInstalling = false;

  while (true) {
    DialogResult res = DialogResult::Cancel;

    if (currentStep == 0) {
      res = ui->showWelcome(manifest);
      if (res == DialogResult::Next)
        currentStep++;
      else if (res == DialogResult::Cancel)
        break;
    } else if (currentStep == 1) {
      res = ui->showDirectorySelect(manifest, currentDir);
      if (res == DialogResult::Next)
        currentStep++;
      else if (res == DialogResult::Back)
        currentStep--;
      else if (res == DialogResult::Cancel)
        break;
    } else if (currentStep == 2) {
      // Execution step
      auto extractCb = [&](InstallerUI *u) -> bool {
        return performExtraction(u, manifest, currentDir);
      };
      res = ui->showProgress(manifest, currentDir, extractCb);
      if (res == DialogResult::Next)
        currentStep++;
      else if (res == DialogResult::Cancel)
        break;
    } else if (currentStep == 3) {
      bool launchApp = true;
      res = ui->showComplete(manifest, launchApp);
      if (res == DialogResult::Next || res == DialogResult::Cancel) {
        if (launchApp) {
          // Launch the installed executable
          std::string exePath =
              (fs::path(currentDir) / manifest.executableName).string();
#ifdef _WIN32
          ShellExecuteA(NULL, "open", exePath.c_str(), NULL, currentDir.c_str(),
                        SW_SHOWNORMAL);
#endif
        }
        break; // Finish
      }
    }
  }

  delete ui;
  return 0;
}

} // namespace installer
} // namespace neuron

#ifdef _WIN32
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
                   LPSTR lpCmdLine, int nCmdShow) {
  return neuron::installer::runInstaller();
}
#else
int main(int argc, char *argv[]) { return neuron::installer::runInstaller(); }
#endif
