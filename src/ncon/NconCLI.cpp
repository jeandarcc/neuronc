#include "neuronc/ncon/NconCLI.h"

#include "neuronc/cli/PackageManager.h"
#include "neuronc/ncon/Builder.h"
#include "neuronc/ncon/Inspect.h"
#include "neuronc/ncon/Reader.h"
#include "neuronc/ncon/Runner.h"

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace neuron::ncon {

namespace fs = std::filesystem;

namespace {

void printUsage(const char *programName) {
  std::cout << "Usage:\n"
            << "  " << programName
            << " build [project-dir|entry.nr] [-o output.ncon]\n"
            << "  " << programName
            << " watch [project-dir|entry.nr] [--hot-reload|--no-hot-reload]\n"
            << "  " << programName
            << " config set hot_reload true|false\n"
            << "  " << programName
            << " hotreload=true|false\n"
            << "  " << programName << " run <program.ncon>\n"
            << "  " << programName << " inspect <program.ncon> [--json]\n";
}

bool parseBoolValue(const std::string &value, bool *outValue) {
  if (outValue == nullptr) {
    return false;
  }
  std::string normalized = value;
  std::transform(normalized.begin(), normalized.end(), normalized.begin(),
                 [](unsigned char ch) {
                   return static_cast<char>(std::tolower(ch));
                 });
  if (normalized == "true" || normalized == "1" || normalized == "yes" ||
      normalized == "on") {
    *outValue = true;
    return true;
  }
  if (normalized == "false" || normalized == "0" || normalized == "no" ||
      normalized == "off") {
    *outValue = false;
    return true;
  }
  return false;
}

std::optional<ProjectConfig> tryLoadConfig(const fs::path &projectRoot) {
  std::vector<std::string> errors;
  ProjectConfig config;
  if (!PackageManager::loadProjectConfig(projectRoot.string(), &config, &errors)) {
    return std::nullopt;
  }
  return config;
}

fs::path watchProjectRootForInput(const fs::path &inputPath) {
  fs::path input = inputPath.empty() ? fs::current_path() : inputPath;
  if (!input.is_absolute()) {
    input = fs::absolute(input);
  }
  if (fs::is_directory(input)) {
    fs::path probe = input;
    for (int depth = 0; depth < 4; ++depth) {
      if (fs::exists(probe / "neuron.toml")) {
        return probe;
      }
      const fs::path parent = probe.parent_path();
      if (parent == probe) {
        break;
      }
      probe = parent;
    }
    return input;
  }
  fs::path probe = input.parent_path();
  for (int depth = 0; depth < 4; ++depth) {
    if (fs::exists(probe / "neuron.toml")) {
      return probe;
    }
    const fs::path parent = probe.parent_path();
    if (parent == probe) {
      break;
    }
    probe = parent;
  }
  return input.parent_path();
}

bool shouldWatchFile(const fs::path &path) {
  const std::string filename = path.filename().string();
  const std::string extension = path.extension().string();
  return extension == ".nr" || filename == "neuron.toml" ||
         filename == "modulecpp.toml";
}

struct WatchSnapshot {
  std::unordered_map<std::string, std::uint64_t> entries;
};

void addWatchPath(const fs::path &path, WatchSnapshot *snapshot,
                  bool filterKnownFiles = true) {
  if (snapshot == nullptr || path.empty()) {
    return;
  }
  std::error_code ec;
  const fs::path normalized = fs::weakly_canonical(path, ec);
  const fs::path effectivePath = ec ? path.lexically_normal() : normalized;

  if (!fs::exists(effectivePath)) {
    snapshot->entries[effectivePath.string()] = 0;
    return;
  }

  if (fs::is_directory(effectivePath)) {
    for (const auto &entry : fs::recursive_directory_iterator(effectivePath, ec)) {
      if (ec || !entry.is_regular_file()) {
        continue;
      }
      if (filterKnownFiles && !shouldWatchFile(entry.path())) {
        continue;
      }
      const auto writeTime = fs::last_write_time(entry.path(), ec);
      snapshot->entries[entry.path().lexically_normal().string()] =
          ec ? 0u : static_cast<std::uint64_t>(writeTime.time_since_epoch().count());
    }
    return;
  }

  const auto writeTime = fs::last_write_time(effectivePath, ec);
  snapshot->entries[effectivePath.string()] =
      ec ? 0u : static_cast<std::uint64_t>(writeTime.time_since_epoch().count());
}

WatchSnapshot captureWatchSnapshot(const fs::path &projectRoot,
                                   const fs::path &inputPath,
                                   const std::optional<ProjectConfig> &config) {
  WatchSnapshot snapshot;
  addWatchPath(projectRoot / "src", &snapshot, true);
  addWatchPath(projectRoot / "modules", &snapshot, true);
  addWatchPath(projectRoot / "neuron.toml", &snapshot);
  if (fs::exists(inputPath) && fs::is_regular_file(inputPath)) {
    addWatchPath(inputPath, &snapshot);
  }

  if (!config.has_value()) {
    return snapshot;
  }

  for (const auto &resource : config->ncon.resources) {
    addWatchPath(projectRoot / resource.sourcePath, &snapshot);
  }
  for (const auto &module : config->ncon.native.modules) {
    if (!module.second.manifestPath.empty()) {
      addWatchPath(projectRoot / module.second.manifestPath, &snapshot);
    }
    if (!module.second.sourceDir.empty()) {
      addWatchPath(projectRoot / module.second.sourceDir, &snapshot, false);
    }
    if (!module.second.artifactWindowsX64.empty()) {
      addWatchPath(projectRoot / module.second.artifactWindowsX64, &snapshot);
    }
    if (!module.second.artifactLinuxX64.empty()) {
      addWatchPath(projectRoot / module.second.artifactLinuxX64, &snapshot);
    }
    if (!module.second.artifactMacosArm64.empty()) {
      addWatchPath(projectRoot / module.second.artifactMacosArm64, &snapshot);
    }
  }

  return snapshot;
}

std::vector<std::string> diffSnapshots(const WatchSnapshot &before,
                                       const WatchSnapshot &after) {
  std::vector<std::string> changed;
  for (const auto &entry : before.entries) {
    auto afterIt = after.entries.find(entry.first);
    if (afterIt == after.entries.end() || afterIt->second != entry.second) {
      changed.push_back(entry.first);
    }
  }
  for (const auto &entry : after.entries) {
    if (before.entries.find(entry.first) == before.entries.end()) {
      changed.push_back(entry.first);
    }
  }
  std::sort(changed.begin(), changed.end());
  changed.erase(std::unique(changed.begin(), changed.end()), changed.end());
  return changed;
}

bool requiresHardRestart(const std::vector<std::string> &changedPaths) {
  for (const auto &path : changedPaths) {
    const fs::path changed(path);
    const std::string filename = changed.filename().string();
    if (filename == "neuron.toml" || filename == "modulecpp.toml") {
      return true;
    }
    if (changed.extension() != ".nr") {
      return true;
    }
    if (changed.string().find("build") != std::string::npos) {
      return true;
    }
  }
  return false;
}

fs::path watchCommandPath(const fs::path &sessionDir) {
  return sessionDir / "watch.command";
}

fs::path watchResponsePath(const fs::path &sessionDir) {
  return sessionDir / "watch.response";
}

bool writeWatchCommand(const fs::path &sessionDir, std::uint64_t sequence,
                       const std::string &commandType,
                       const fs::path &containerPath, std::string *outError) {
  std::ofstream out(watchCommandPath(sessionDir),
                    std::ios::binary | std::ios::trunc);
  if (!out.is_open()) {
    if (outError != nullptr) {
      *outError = "failed to write watch command";
    }
    return false;
  }
  out << sequence << ' ' << commandType << ' ' << containerPath.string() << '\n';
  return out.good();
}

struct WatchResponse {
  std::uint64_t sequence = 0;
  std::string status;
  std::string message;
};

bool readWatchResponse(const fs::path &sessionDir, WatchResponse *outResponse) {
  if (outResponse == nullptr) {
    return false;
  }
  const fs::path responsePath = watchResponsePath(sessionDir);
  if (!fs::exists(responsePath)) {
    return false;
  }

  std::ifstream in(responsePath, std::ios::binary);
  if (!in.is_open()) {
    return false;
  }

  std::string line;
  std::getline(in, line);
  if (line.empty()) {
    return false;
  }

  std::istringstream parts(line);
  if (!(parts >> outResponse->sequence >> outResponse->status)) {
    return false;
  }
  std::getline(parts, outResponse->message);
  if (!outResponse->message.empty() && outResponse->message.front() == ' ') {
    outResponse->message.erase(outResponse->message.begin());
  }
  while (!outResponse->message.empty() &&
         (outResponse->message.back() == '\r' ||
          outResponse->message.back() == '\n' ||
          outResponse->message.back() == ' ' ||
          outResponse->message.back() == '\t')) {
    outResponse->message.pop_back();
  }
  return true;
}

bool waitForWatchResponse(const fs::path &sessionDir, std::uint64_t sequence,
                          int timeoutMs, WatchResponse *outResponse) {
  const auto deadline = std::chrono::steady_clock::now() +
                        std::chrono::milliseconds(timeoutMs);
  while (std::chrono::steady_clock::now() < deadline) {
    WatchResponse response;
    if (readWatchResponse(sessionDir, &response) &&
        response.sequence == sequence) {
      if (outResponse != nullptr) {
        *outResponse = std::move(response);
      }
      return true;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }
  return false;
}

#ifdef _WIN32
bool resolveSelfExecutable(const char *invokerPath, fs::path *outPath,
                          std::string *outError) {
  if (outPath == nullptr) {
    if (outError != nullptr) {
      *outError = "internal error: null executable output";
    }
    return false;
  }

  std::vector<wchar_t> buffer(MAX_PATH, L'\0');
  for (;;) {
    const DWORD written = GetModuleFileNameW(nullptr, buffer.data(),
                                             static_cast<DWORD>(buffer.size()));
    if (written == 0) {
      break;
    }
    if (written < buffer.size() - 1) {
      *outPath = fs::path(std::wstring(buffer.data(), written));
      return true;
    }
    buffer.resize(buffer.size() * 2, L'\0');
  }

  if (invokerPath != nullptr && *invokerPath != '\0') {
    *outPath = fs::path(invokerPath);
    return true;
  }

  if (outError != nullptr) {
    *outError = "failed to resolve current executable";
  }
  return false;
}

bool spawnWatchSessionProcess(const fs::path &sessionDir,
                              const fs::path &containerPath,
                              const fs::path &selfPath,
                              bool viaNeuronInvoker,
                              PROCESS_INFORMATION *outProcess,
                              std::string *outError) {
  if (outProcess == nullptr) {
    if (outError != nullptr) {
      *outError = "internal error: null process output";
    }
    return false;
  }

  std::string commandLine = "\"" + selfPath.string() + "\" ";
  if (viaNeuronInvoker) {
    commandLine += "ncon ";
  }
  commandLine += "__watch_session \"" + sessionDir.string() + "\" \"" +
                 containerPath.string() + "\"";

  STARTUPINFOA startupInfo{};
  startupInfo.cb = sizeof(startupInfo);
  PROCESS_INFORMATION processInfo{};
  std::vector<char> commandBuffer(commandLine.begin(), commandLine.end());
  commandBuffer.push_back('\0');
  if (!CreateProcessA(nullptr, commandBuffer.data(), nullptr, nullptr, TRUE, 0,
                      nullptr, nullptr, &startupInfo, &processInfo)) {
    if (outError != nullptr) {
      *outError = "failed to launch watch session process";
    }
    return false;
  }

  *outProcess = processInfo;
  return true;
}

void closeProcessHandles(PROCESS_INFORMATION *processInfo) {
  if (processInfo == nullptr) {
    return;
  }
  if (processInfo->hThread != nullptr) {
    CloseHandle(processInfo->hThread);
    processInfo->hThread = nullptr;
  }
  if (processInfo->hProcess != nullptr) {
    CloseHandle(processInfo->hProcess);
    processInfo->hProcess = nullptr;
  }
}

bool isProcessRunning(const PROCESS_INFORMATION &processInfo) {
  if (processInfo.hProcess == nullptr) {
    return false;
  }
  return WaitForSingleObject(processInfo.hProcess, 0) == WAIT_TIMEOUT;
}

void stopProcess(PROCESS_INFORMATION *processInfo) {
  if (processInfo == nullptr || processInfo->hProcess == nullptr) {
    return;
  }
  TerminateProcess(processInfo->hProcess, 0);
  WaitForSingleObject(processInfo->hProcess, 5000);
  closeProcessHandles(processInfo);
}
#endif

int runConfigSet(int argc, char *argv[]) {
  if (argc != 5 || std::string(argv[2]) != "set") {
    std::cerr << "Usage: " << argv[0]
              << " config set hot_reload true|false" << std::endl;
    return 1;
  }
  const std::string key = argv[3];
  if (key != "hot_reload") {
    std::cerr << "Unsupported ncon config key: " << key << std::endl;
    return 1;
  }

  bool enabled = false;
  if (!parseBoolValue(argv[4], &enabled)) {
    std::cerr << "Invalid hot_reload value: " << argv[4] << std::endl;
    return 1;
  }

  std::vector<std::string> errors;
  ProjectConfig config;
  if (!PackageManager::loadProjectConfig(fs::current_path().string(), &config,
                                         &errors)) {
    for (const auto &error : errors) {
      std::cerr << error << std::endl;
    }
    return 1;
  }
  config.ncon.hotReload = enabled;

  std::string message;
  if (!PackageManager::writeProjectConfig(fs::current_path().string(), config,
                                          &message)) {
    std::cerr << message << std::endl;
    return 1;
  }

  std::cout << "Updated ncon.hot_reload = " << (enabled ? "true" : "false")
            << std::endl;
  return 0;
}

int runHotReloadAlias(const std::string &assignment, char *argv[]) {
  const std::size_t equals = assignment.find('=');
  if (equals == std::string::npos) {
    std::cerr << "Usage: " << argv[0] << " hotreload=true|false" << std::endl;
    return 1;
  }
  bool enabled = false;
  if (!parseBoolValue(assignment.substr(equals + 1), &enabled)) {
    std::cerr << "Invalid hotreload value" << std::endl;
    return 1;
  }

  const std::string enabledText = enabled ? "true" : "false";
  char command0[] = "config";
  char command1[] = "set";
  char command2[] = "hot_reload";
  std::vector<char> value(enabledText.begin(), enabledText.end());
  value.push_back('\0');
  char *configArgv[] = {argv[0], command0, command1, command2, value.data()};
  return runConfigSet(5, configArgv);
}

int runBuild(int argc, char *argv[]) {
  BuildRequest request;
  bool expectingOutputPath = false;
  for (int i = 2; i < argc; ++i) {
    const std::string arg = argv[i];
    if (expectingOutputPath) {
      request.outputPath = arg;
      expectingOutputPath = false;
      continue;
    }
    if (arg == "-o" || arg == "--output") {
      expectingOutputPath = true;
      continue;
    }
    if (!request.inputPath.empty()) {
      std::cerr << "ncon build accepts a single input path" << std::endl;
      return 1;
    }
    request.inputPath = arg;
  }

  if (expectingOutputPath) {
    std::cerr << "missing value for -o/--output" << std::endl;
    return 1;
  }

  fs::path outputPath;
  std::string error;
  if (!buildContainerFromInput(request, &outputPath, &error)) {
    std::cerr << error << std::endl;
    return 1;
  }

  std::cout << "Built NCON container: " << outputPath.string() << std::endl;
  return 0;
}

int runWatch(int argc, char *argv[], const char *invokerPath) {
  fs::path inputPath = fs::current_path();
  std::optional<bool> hotReloadOverride;
  for (int i = 2; i < argc; ++i) {
    const std::string arg = argv[i];
    if (arg == "--hot-reload") {
      hotReloadOverride = true;
      continue;
    }
    if (arg == "--no-hot-reload") {
      hotReloadOverride = false;
      continue;
    }
    if (!inputPath.empty() && inputPath != fs::current_path()) {
      std::cerr << "watch accepts a single input path" << std::endl;
      return 1;
    }
    inputPath = arg;
  }

  fs::path absoluteInput =
      inputPath.empty() ? fs::current_path() : fs::absolute(inputPath);
  const fs::path projectRoot = watchProjectRootForInput(absoluteInput);
  const auto config = tryLoadConfig(projectRoot);
  const bool hotReloadEnabled =
      hotReloadOverride.has_value()
          ? *hotReloadOverride
          : (config.has_value() ? config->ncon.hotReload : false);

  const std::size_t sessionKey =
      std::hash<std::string>{}(projectRoot.lexically_normal().string());
  const fs::path sessionDir =
      fs::temp_directory_path() / "Neuron" / "ncon-watch" /
      std::to_string(sessionKey);
  std::error_code ec;
  fs::create_directories(sessionDir, ec);
  if (ec) {
    std::cerr << "Failed to create watch session directory: " << ec.message()
              << std::endl;
    return 1;
  }

  const fs::path sessionContainer = sessionDir / "session.ncon";
  auto snapshot = captureWatchSnapshot(projectRoot, absoluteInput, config);

  BuildRequest request;
  request.inputPath = absoluteInput;
  request.outputPath = sessionContainer;
  std::string error;
  fs::path builtPath;
  if (!buildContainerFromInput(request, &builtPath, &error)) {
    std::cerr << error << std::endl;
    return 1;
  }

  std::cout << "Watching " << projectRoot.string() << " (hot_reload="
            << (hotReloadEnabled ? "true" : "false") << ")" << std::endl;

#ifdef _WIN32
  fs::path selfPath;
  if (!resolveSelfExecutable(invokerPath, &selfPath, &error)) {
    std::cerr << error << std::endl;
    return 1;
  }
  const bool viaNeuronInvoker =
      invokerPath != nullptr && !fs::path(invokerPath).empty() &&
      fs::path(invokerPath).stem() != "ncon";
  fs::path activeContainer = sessionContainer;
  std::uint64_t watchCommandSequence = 0;

  PROCESS_INFORMATION processInfo{};
  if (!spawnWatchSessionProcess(sessionDir, activeContainer, selfPath,
                                viaNeuronInvoker, &processInfo, &error)) {
    std::cerr << error << std::endl;
    return 1;
  }

  for (;;) {
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    const auto latestConfig = tryLoadConfig(projectRoot);
    const auto nextSnapshot =
        captureWatchSnapshot(projectRoot, absoluteInput, latestConfig);
    const auto changedPaths = diffSnapshots(snapshot, nextSnapshot);
    if (!changedPaths.empty()) {
      BuildRequest rebuildRequest;
      rebuildRequest.inputPath = absoluteInput;
      rebuildRequest.outputPath =
          sessionDir /
          ("reload-" + std::to_string(static_cast<unsigned long long>(
                            watchCommandSequence + 1)) +
           ".ncon");
      fs::path nextContainer;
      if (!buildContainerFromInput(rebuildRequest, &nextContainer, &error)) {
        std::cerr << error << std::endl;
        snapshot = nextSnapshot;
        continue;
      }

      const bool patchAttempted =
          hotReloadEnabled && !requiresHardRestart(changedPaths) &&
          isProcessRunning(processInfo);
      bool patchApplied = false;
      if (patchAttempted) {
        std::cout << "Compatible change detected; applying hot reload."
                  << std::endl;
        ++watchCommandSequence;
        if (!writeWatchCommand(sessionDir, watchCommandSequence, "patch",
                               nextContainer, &error)) {
          std::cerr << error << std::endl;
        } else {
          WatchResponse response;
          if (waitForWatchResponse(sessionDir, watchCommandSequence, 15000,
                                   &response) &&
              response.status == "applied") {
            patchApplied = true;
            activeContainer = nextContainer;
          } else {
            std::cout << "Hot reload fell back to restart."
                      << (response.message.empty() ? std::string()
                                                  : " (" + response.message + ")")
                      << std::endl;
          }
        }
      } else {
        std::cout << "Hard restart required; rebuilding session." << std::endl;
      }

      if (!patchApplied) {
        if (isProcessRunning(processInfo)) {
          stopProcess(&processInfo);
        } else {
          closeProcessHandles(&processInfo);
        }

        activeContainer = nextContainer;
        if (!spawnWatchSessionProcess(sessionDir, activeContainer, selfPath,
                                      viaNeuronInvoker, &processInfo, &error)) {
          std::cerr << error << std::endl;
          return 1;
        }
      }

      snapshot = nextSnapshot;
      continue;
    }

    if (!isProcessRunning(processInfo)) {
      closeProcessHandles(&processInfo);
      if (!spawnWatchSessionProcess(sessionDir, activeContainer, selfPath,
                                    viaNeuronInvoker, &processInfo, &error)) {
        std::cerr << error << std::endl;
        return 1;
      }
    }
  }
#else
  (void)hotReloadEnabled;
  (void)invokerPath;
  return runContainerDirect(sessionContainer);
#endif
}

int runInspect(int argc, char *argv[]) {
  if (argc < 3) {
    std::cerr << "Usage: " << argv[0] << " inspect <program.ncon> [--json]"
              << std::endl;
    return 1;
  }

  fs::path inputPath;
  bool json = false;
  for (int i = 2; i < argc; ++i) {
    const std::string arg = argv[i];
    if (arg == "--json") {
      json = true;
      continue;
    }
    if (!inputPath.empty()) {
      std::cerr << "inspect accepts a single input container" << std::endl;
      return 1;
    }
    inputPath = arg;
  }

  if (inputPath.empty()) {
    std::cerr << "inspect requires an input container" << std::endl;
    return 1;
  }

  ContainerData container;
  std::string error;
  if (!readContainer(inputPath, &container, &error)) {
    std::cerr << error << std::endl;
    return 1;
  }

  std::cout << (json ? inspectContainerJson(container)
                     : inspectContainerHuman(container));
  return 0;
}

int runContainer(int argc, char *argv[], const char *invokerPath) {
  if (argc != 3) {
    std::cerr << "Usage: " << argv[0] << " run <program.ncon>" << std::endl;
    return 1;
  }

  return runContainerCommand(fs::path(argv[2]), invokerPath);
}

} // namespace

int runCli(int argc, char *argv[], const char *invokerPath) {
  if (argc < 2) {
    printUsage(argv[0]);
    return 1;
  }

  const std::string command = argv[1];
  if (command == "help" || command == "--help" || command == "-h") {
    printUsage(argv[0]);
    return 0;
  }
  if (command == "build") {
    return runBuild(argc, argv);
  }
  if (command == "watch") {
    return runWatch(argc, argv, invokerPath);
  }
  if (command == "config") {
    return runConfigSet(argc, argv);
  }
  if (command == "inspect") {
    return runInspect(argc, argv);
  }
  if (command == "__watch_session") {
    if (argc != 4) {
      std::cerr << "watch session helper requires a session dir and container path"
                << std::endl;
      return 1;
    }
    return runWatchSessionDirect(fs::path(argv[2]), fs::path(argv[3]));
  }
  if (command == "__sandbox_run") {
    if (argc != 3) {
      std::cerr << "sandbox helper requires a single staged container path"
                << std::endl;
      return 1;
    }
    return runContainerDirect(fs::path(argv[2]));
  }
  if (command == "run") {
    return runContainer(argc, argv, invokerPath);
  }
  if (command.rfind("hotreload=", 0) == 0) {
    return runHotReloadAlias(command, argv);
  }

  std::cerr << "Unknown ncon command: " << command << std::endl;
  printUsage(argv[0]);
  return 1;
}

} // namespace neuron::ncon
