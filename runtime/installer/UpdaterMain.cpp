#include "Ed25519.h"
#include "UpdaterUI.h"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <winhttp.h>

// Keep updater as a single translation unit for standalone compilation.
#include "UpdaterUI_Win32.cpp"
#endif

namespace fs = std::filesystem;

namespace neuron::installer {

namespace {

struct UpdateRecord {
  std::string version;
  std::string url;
  std::string signature;
  std::string notes;
};

struct UpdaterOptions {
  std::string appcastUrl;
  std::string currentVersion = "0.0.0";
  std::string channel = "stable";
  std::string publicKey;
  fs::path targetPath;
  bool silent = false;
};

std::string trim(const std::string &text) {
  const auto first = text.find_first_not_of(" \t\r\n");
  if (first == std::string::npos) {
    return {};
  }
  const auto last = text.find_last_not_of(" \t\r\n");
  return text.substr(first, last - first + 1u);
}

bool endsWithInsensitive(const std::string &value, const std::string &suffix) {
  if (suffix.size() > value.size()) {
    return false;
  }
  const size_t offset = value.size() - suffix.size();
  for (size_t i = 0; i < suffix.size(); ++i) {
    const char a = static_cast<char>(std::tolower(
        static_cast<unsigned char>(value[offset + i])));
    const char b = static_cast<char>(std::tolower(
        static_cast<unsigned char>(suffix[i])));
    if (a != b) {
      return false;
    }
  }
  return true;
}

#ifdef _WIN32
std::wstring utf8ToWide(const std::string &text) {
  if (text.empty()) {
    return {};
  }
  const int length = MultiByteToWideChar(CP_UTF8, 0, text.c_str(),
                                         static_cast<int>(text.size()), nullptr,
                                         0);
  if (length <= 0) {
    return {};
  }
  std::wstring result(static_cast<size_t>(length), L'\0');
  MultiByteToWideChar(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()),
                      result.data(), length);
  return result;
}

std::string wideToUtf8(const std::wstring &text) {
  if (text.empty()) {
    return {};
  }
  const int length = WideCharToMultiByte(CP_UTF8, 0, text.c_str(),
                                         static_cast<int>(text.size()), nullptr,
                                         0, nullptr, nullptr);
  if (length <= 0) {
    return {};
  }
  std::string result(static_cast<size_t>(length), '\0');
  WideCharToMultiByte(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()),
                      result.data(), length, nullptr, nullptr);
  return result;
}

std::string getExecutablePath() {
  char path[MAX_PATH];
  const DWORD len = GetModuleFileNameA(nullptr, path, MAX_PATH);
  if (len == 0 || len >= MAX_PATH) {
    return {};
  }
  return std::string(path, len);
}
#else
std::string getExecutablePath() { return {}; }
#endif

std::string extractJsonStringField(const std::string &json,
                                   const std::string &key) {
  const std::string needle = "\"" + key + "\"";
  const size_t keyPos = json.find(needle);
  if (keyPos == std::string::npos) {
    return {};
  }
  size_t colonPos = json.find(':', keyPos + needle.size());
  if (colonPos == std::string::npos) {
    return {};
  }
  size_t valueStart = json.find('"', colonPos + 1u);
  if (valueStart == std::string::npos) {
    return {};
  }
  size_t valueEnd = json.find('"', valueStart + 1u);
  if (valueEnd == std::string::npos || valueEnd <= valueStart) {
    return {};
  }
  return json.substr(valueStart + 1u, valueEnd - valueStart - 1u);
}

std::string extractJsonObject(const std::string &json, const std::string &key) {
  const std::string needle = "\"" + key + "\"";
  const size_t keyPos = json.find(needle);
  if (keyPos == std::string::npos) {
    return {};
  }
  size_t objectStart = json.find('{', keyPos + needle.size());
  if (objectStart == std::string::npos) {
    return {};
  }

  int depth = 0;
  for (size_t i = objectStart; i < json.size(); ++i) {
    const char ch = json[i];
    if (ch == '{') {
      depth++;
    } else if (ch == '}') {
      depth--;
      if (depth == 0) {
        return json.substr(objectStart, i - objectStart + 1u);
      }
    }
  }
  return {};
}

bool parseVersionParts(const std::string &version, int *major, int *minor,
                       int *patch) {
  if (major == nullptr || minor == nullptr || patch == nullptr) {
    return false;
  }

  std::vector<std::string> parts;
  std::string token;
  std::istringstream stream(version);
  while (std::getline(stream, token, '.')) {
    parts.push_back(token);
  }
  if (parts.size() != 3u) {
    return false;
  }

  auto parsePart = [](const std::string &part, int *out) -> bool {
    if (part.empty() || out == nullptr) {
      return false;
    }
    for (char ch : part) {
      if (ch < '0' || ch > '9') {
        return false;
      }
    }
    try {
      *out = std::stoi(part);
      return *out >= 0;
    } catch (...) {
      return false;
    }
  };

  return parsePart(parts[0], major) && parsePart(parts[1], minor) &&
         parsePart(parts[2], patch);
}

int compareSemver(const std::string &left, const std::string &right) {
  int la = 0;
  int lb = 0;
  int lc = 0;
  int ra = 0;
  int rb = 0;
  int rc = 0;
  if (!parseVersionParts(left, &la, &lb, &lc) ||
      !parseVersionParts(right, &ra, &rb, &rc)) {
    return left.compare(right);
  }
  if (la != ra) {
    return la < ra ? -1 : 1;
  }
  if (lb != rb) {
    return lb < rb ? -1 : 1;
  }
  if (lc != rc) {
    return lc < rc ? -1 : 1;
  }
  return 0;
}

bool parseAppcastRecord(const std::string &appcastText,
                        const std::string &channel, UpdateRecord *outRecord,
                        std::string *outError) {
  if (outRecord == nullptr) {
    if (outError != nullptr) {
      *outError = "internal error: null appcast output";
    }
    return false;
  }

  std::string scope = appcastText;
  const std::string channelsObject = extractJsonObject(appcastText, "channels");
  if (!channelsObject.empty()) {
    const std::string channelObject = extractJsonObject(channelsObject, channel);
    if (!channelObject.empty()) {
      scope = channelObject;
    }
  }

  UpdateRecord record;
  record.version = trim(extractJsonStringField(scope, "version"));
  record.url = trim(extractJsonStringField(scope, "url"));
  record.signature = trim(extractJsonStringField(scope, "signature"));
  record.notes = trim(extractJsonStringField(scope, "notes"));

  if (record.version.empty()) {
    if (outError != nullptr) {
      *outError = "appcast missing required field: version";
    }
    return false;
  }
  if (record.url.empty()) {
    if (outError != nullptr) {
      *outError = "appcast missing required field: url";
    }
    return false;
  }
  if (record.signature.empty()) {
    if (outError != nullptr) {
      *outError = "appcast missing required field: signature";
    }
    return false;
  }

  *outRecord = std::move(record);
  return true;
}

#ifdef _WIN32
bool downloadUrl(const std::string &url, std::vector<uint8_t> *outBytes,
                 std::string *outError, UpdaterUI *ui = nullptr,
                 int progressBase = 0, int progressSpan = 100) {
  if (outBytes == nullptr) {
    if (outError != nullptr) {
      *outError = "internal error: null download output";
    }
    return false;
  }

  const std::wstring wideUrl = utf8ToWide(url);
  if (wideUrl.empty()) {
    if (outError != nullptr) {
      *outError = "invalid update URL";
    }
    return false;
  }

  URL_COMPONENTS parts{};
  parts.dwStructSize = sizeof(parts);
  parts.dwSchemeLength = static_cast<DWORD>(-1);
  parts.dwHostNameLength = static_cast<DWORD>(-1);
  parts.dwUrlPathLength = static_cast<DWORD>(-1);
  parts.dwExtraInfoLength = static_cast<DWORD>(-1);
  if (!WinHttpCrackUrl(wideUrl.c_str(), static_cast<DWORD>(wideUrl.size()), 0,
                       &parts)) {
    if (outError != nullptr) {
      *outError = "failed to parse update URL";
    }
    return false;
  }

  const std::wstring host(parts.lpszHostName, parts.dwHostNameLength);
  std::wstring path(parts.lpszUrlPath, parts.dwUrlPathLength);
  if (parts.dwExtraInfoLength > 0) {
    path += std::wstring(parts.lpszExtraInfo, parts.dwExtraInfoLength);
  }

  HINTERNET session = WinHttpOpen(L"NeuronUpdater/1.0",
                                  WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
                                  WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS,
                                  0);
  if (session == nullptr) {
    if (outError != nullptr) {
      *outError = "WinHttpOpen failed";
    }
    return false;
  }

  HINTERNET connect =
      WinHttpConnect(session, host.c_str(), parts.nPort, 0);
  if (connect == nullptr) {
    WinHttpCloseHandle(session);
    if (outError != nullptr) {
      *outError = "WinHttpConnect failed";
    }
    return false;
  }

  const DWORD requestFlags =
      (parts.nScheme == INTERNET_SCHEME_HTTPS) ? WINHTTP_FLAG_SECURE : 0u;
  HINTERNET request =
      WinHttpOpenRequest(connect, L"GET", path.c_str(), nullptr,
                         WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
                         requestFlags);
  if (request == nullptr) {
    WinHttpCloseHandle(connect);
    WinHttpCloseHandle(session);
    if (outError != nullptr) {
      *outError = "WinHttpOpenRequest failed";
    }
    return false;
  }

  bool ok = false;
  uint64_t contentLength = 0;
  uint64_t totalRead = 0;
  do {
    if (!WinHttpSendRequest(request, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                            WINHTTP_NO_REQUEST_DATA, 0, 0, 0)) {
      if (outError != nullptr) {
        *outError = "WinHttpSendRequest failed";
      }
      break;
    }
    if (!WinHttpReceiveResponse(request, nullptr)) {
      if (outError != nullptr) {
        *outError = "WinHttpReceiveResponse failed";
      }
      break;
    }

    DWORD statusCode = 0;
    DWORD statusSize = sizeof(statusCode);
    if (WinHttpQueryHeaders(request, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                            WINHTTP_HEADER_NAME_BY_INDEX, &statusCode, &statusSize,
                            WINHTTP_NO_HEADER_INDEX)) {
      if (statusCode < 200 || statusCode >= 300) {
        if (outError != nullptr) {
          *outError = "HTTP request failed with status " +
                      std::to_string(statusCode);
        }
        break;
      }
    }

    DWORD contentLength32 = 0;
    DWORD contentLengthSize = sizeof(contentLength32);
    if (WinHttpQueryHeaders(request,
                            WINHTTP_QUERY_CONTENT_LENGTH |
                                WINHTTP_QUERY_FLAG_NUMBER,
                            WINHTTP_HEADER_NAME_BY_INDEX, &contentLength32,
                            &contentLengthSize, WINHTTP_NO_HEADER_INDEX)) {
      contentLength = contentLength32;
    }

    outBytes->clear();
    while (true) {
      DWORD available = 0;
      if (!WinHttpQueryDataAvailable(request, &available)) {
        if (outError != nullptr) {
          *outError = "WinHttpQueryDataAvailable failed";
        }
        break;
      }
      if (available == 0) {
        ok = true;
        break;
      }

      const size_t oldSize = outBytes->size();
      outBytes->resize(oldSize + available);
      DWORD downloaded = 0;
      if (!WinHttpReadData(request, outBytes->data() + oldSize, available,
                           &downloaded)) {
        if (outError != nullptr) {
          *outError = "WinHttpReadData failed";
        }
        break;
      }
      outBytes->resize(oldSize + downloaded);
      totalRead += downloaded;

      if (ui != nullptr && contentLength > 0) {
        const double ratio =
            static_cast<double>(totalRead) / static_cast<double>(contentLength);
        const int percent = progressBase +
                            static_cast<int>(ratio * static_cast<double>(progressSpan));
        ui->setProgress(percent);
      }
    }
  } while (false);

  WinHttpCloseHandle(request);
  WinHttpCloseHandle(connect);
  WinHttpCloseHandle(session);
  return ok;
}
#else
bool downloadUrl(const std::string &, std::vector<uint8_t> *, std::string *outError,
                 UpdaterUI *, int, int) {
  if (outError != nullptr) {
    *outError = "updater is only supported on Windows";
  }
  return false;
}
#endif

bool parseUpdaterArgs(int argc, char **argv, UpdaterOptions *outOptions,
                      std::string *outError) {
  if (outOptions == nullptr) {
    if (outError != nullptr) {
      *outError = "internal error: null options output";
    }
    return false;
  }

  UpdaterOptions options;
  for (int i = 1; i < argc; ++i) {
    const std::string arg = argv[i];
    if (arg == "--appcast" && i + 1 < argc) {
      options.appcastUrl = argv[++i];
      continue;
    }
    if (arg == "--current-version" && i + 1 < argc) {
      options.currentVersion = argv[++i];
      continue;
    }
    if (arg == "--channel" && i + 1 < argc) {
      options.channel = argv[++i];
      continue;
    }
    if (arg == "--public-key" && i + 1 < argc) {
      options.publicKey = argv[++i];
      continue;
    }
    if (arg == "--target" && i + 1 < argc) {
      options.targetPath = fs::path(argv[++i]);
      continue;
    }
    if (arg == "--silent") {
      options.silent = true;
      continue;
    }
  }

  if (options.appcastUrl.empty()) {
    if (outError != nullptr) {
      *outError = "missing --appcast <url>";
    }
    return false;
  }
  if (options.publicKey.empty()) {
    if (outError != nullptr) {
      *outError = "missing --public-key <hex-or-base64-ed25519-pubkey>";
    }
    return false;
  }

  if (options.targetPath.empty()) {
    const fs::path selfPath = fs::path(getExecutablePath());
    if (!selfPath.empty()) {
      const std::string selfName = selfPath.filename().string();
      if (endsWithInsensitive(selfName, "-Updater.exe")) {
        const std::string appName =
            selfName.substr(0, selfName.size() - std::string("-Updater.exe").size()) +
            ".exe";
        options.targetPath = selfPath.parent_path() / appName;
      }
    }
  }

  if (options.targetPath.empty()) {
    if (outError != nullptr) {
      *outError = "missing --target <installed-executable-path>";
    }
    return false;
  }

  options.targetPath = fs::absolute(options.targetPath);
  *outOptions = std::move(options);
  return true;
}

#ifdef _WIN32
bool replaceTargetExecutable(const fs::path &targetPath, const fs::path &newPath,
                             std::string *outError, bool *outDeferredReboot) {
  if (outDeferredReboot != nullptr) {
    *outDeferredReboot = false;
  }

  const std::wstring targetWide = utf8ToWide(targetPath.string());
  const std::wstring newWide = utf8ToWide(newPath.string());
  if (targetWide.empty() || newWide.empty()) {
    if (outError != nullptr) {
      *outError = "failed to convert target path for replacement";
    }
    return false;
  }

  if (MoveFileExW(newWide.c_str(), targetWide.c_str(),
                  MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
    return true;
  }

  if (MoveFileExW(newWide.c_str(), targetWide.c_str(),
                  MOVEFILE_REPLACE_EXISTING | MOVEFILE_DELAY_UNTIL_REBOOT)) {
    if (outDeferredReboot != nullptr) {
      *outDeferredReboot = true;
    }
    return true;
  }

  if (outError != nullptr) {
    *outError = "failed to replace target executable";
  }
  return false;
}
#else
bool replaceTargetExecutable(const fs::path &, const fs::path &, std::string *outError,
                             bool *outDeferredReboot) {
  if (outDeferredReboot != nullptr) {
    *outDeferredReboot = false;
  }
  if (outError != nullptr) {
    *outError = "updater is only supported on Windows";
  }
  return false;
}
#endif

} // namespace

int runUpdater(int argc, char **argv) {
  UpdaterOptions options;
  std::string parseError;
  if (!parseUpdaterArgs(argc, argv, &options, &parseError)) {
    UpdaterUI *ui = createUpdaterUI();
    if (ui != nullptr) {
      ui->showError("Updater Error", parseError);
      delete ui;
    }
    return 1;
  }

  UpdaterUI *ui = createUpdaterUI();
  if (ui == nullptr) {
    return 1;
  }

  ui->setStatus("Checking update feed...");
  ui->setProgress(5);

  std::vector<uint8_t> appcastBytes;
  std::string downloadError;
  if (!downloadUrl(options.appcastUrl, &appcastBytes, &downloadError, ui, 5, 15)) {
    ui->showError("Updater Error", "Failed to download appcast: " + downloadError);
    delete ui;
    return 1;
  }

  const std::string appcastText(appcastBytes.begin(), appcastBytes.end());
  UpdateRecord record;
  std::string appcastError;
  if (!parseAppcastRecord(appcastText, options.channel, &record, &appcastError)) {
    ui->showError("Updater Error", "Invalid appcast payload: " + appcastError);
    delete ui;
    return 1;
  }

  if (compareSemver(record.version, options.currentVersion) <= 0) {
    ui->showInfo("Updater", "No updates available.");
    delete ui;
    return 0;
  }

  UpdatePromptInfo prompt;
  prompt.currentVersion = options.currentVersion;
  prompt.latestVersion = record.version;
  prompt.notes = record.notes;

  if (!options.silent && !ui->confirmUpdate(prompt)) {
    delete ui;
    return 0;
  }

  ui->setStatus("Downloading update package...");
  ui->setProgress(20);

  std::vector<uint8_t> updateBytes;
  if (!downloadUrl(record.url, &updateBytes, &downloadError, ui, 20, 60)) {
    ui->showError("Updater Error", "Failed to download update: " + downloadError);
    delete ui;
    return 1;
  }

  ui->setStatus("Verifying Ed25519 signature...");
  ui->setProgress(85);

  std::string verifyError;
  if (!crypto::verifyEd25519Encoded(updateBytes, record.signature,
                                    options.publicKey, &verifyError)) {
    ui->showError("Updater Error",
                  "Update signature verification failed: " + verifyError);
    delete ui;
    return 1;
  }

  const fs::path tempUpdatePath =
      options.targetPath.parent_path() /
      (options.targetPath.filename().string() + ".new");
  std::ofstream out(tempUpdatePath, std::ios::binary | std::ios::trunc);
  if (!out.is_open()) {
    ui->showError("Updater Error", "Failed to write temporary update file.");
    delete ui;
    return 1;
  }
  out.write(reinterpret_cast<const char *>(updateBytes.data()),
            static_cast<std::streamsize>(updateBytes.size()));
  out.close();

  bool deferredReboot = false;
  std::string replaceError;
  if (!replaceTargetExecutable(options.targetPath, tempUpdatePath, &replaceError,
                               &deferredReboot)) {
    ui->showError("Updater Error", replaceError);
    delete ui;
    return 1;
  }

  ui->setProgress(100);
  ui->setStatus("Update installed.");
  if (deferredReboot) {
    ui->showInfo("Updater", "Update queued. Restart Windows to finish install.");
  } else {
    ui->showInfo("Updater", "Update installed successfully.");
  }
  delete ui;
  return 0;
}

} // namespace neuron::installer

#ifdef _WIN32
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {
  return neuron::installer::runUpdater(__argc, __argv);
}
#else
int main(int argc, char **argv) { return neuron::installer::runUpdater(argc, argv); }
#endif

