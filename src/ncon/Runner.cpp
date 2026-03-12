#include "neuronc/ncon/Runner.h"

#include "neuronc/ncon/Format.h"
#include "neuronc/ncon/Manifest.h"
#include "neuronc/ncon/Reader.h"
#include "neuronc/ncon/Sandbox.h"
#include "neuronc/ncon/Sha256.h"
#include "neuronc/ncon/VM.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
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

bool readFileBytes(const fs::path &path, std::vector<uint8_t> *outBytes,
                   std::string *outError) {
  if (outBytes == nullptr) {
    if (outError != nullptr) {
      *outError = "internal error: null byte output";
    }
    return false;
  }

  std::ifstream in(path, std::ios::binary);
  if (!in.is_open()) {
    if (outError != nullptr) {
      *outError = "failed to open file: " + path.string();
    }
    return false;
  }

  in.seekg(0, std::ios::end);
  const std::streamoff size = in.tellg();
  in.seekg(0, std::ios::beg);
  if (size < 0) {
    if (outError != nullptr) {
      *outError = "failed to stat file: " + path.string();
    }
    return false;
  }

  outBytes->resize(static_cast<size_t>(size));
  if (!outBytes->empty()) {
    in.read(reinterpret_cast<char *>(outBytes->data()),
            static_cast<std::streamsize>(outBytes->size()));
    if (in.gcount() != static_cast<std::streamsize>(outBytes->size())) {
      if (outError != nullptr) {
        *outError = "failed to read file: " + path.string();
      }
      return false;
    }
  }

  return true;
}

std::string containerHashForPath(const fs::path &path) {
  std::string digest;
  std::string error;
  if (!sha256FileHex(path, &digest, &error)) {
    return "unknown";
  }
  return digest;
}

bool isSafeResourceId(const std::string &id) {
  const fs::path logicalPath(id);
  if (logicalPath.is_absolute()) {
    return false;
  }
  for (const auto &part : logicalPath) {
    const std::string element = part.string();
    if (element == ".." || element.find(':') != std::string::npos) {
      return false;
    }
  }
  return true;
}

bool isNativeModuleResourceId(const std::string &id) {
  return id.rfind("__nativemodules__/", 0) == 0;
}

bool materializeResources(const ContainerData &container,
                          SandboxContext *sandbox,
                          std::string *outError) {
  if (sandbox == nullptr) {
    if (outError != nullptr) {
      *outError = "internal error: null sandbox context";
    }
    return false;
  }

  const fs::path resourceRoot = sandbox->resourceDirectory.empty()
                                    ? (sandbox->workDirectory / "res")
                                    : sandbox->resourceDirectory;
  std::error_code ec;
  fs::create_directories(resourceRoot, ec);
  if (ec) {
    if (outError != nullptr) {
      *outError = "failed to create resource directory: " + ec.message();
    }
    return false;
  }

  for (const auto &resource : container.resources) {
    if (isNativeModuleResourceId(resource.id)) {
      continue;
    }
    if (!isSafeResourceId(resource.id)) {
      if (outError != nullptr) {
        *outError = "unsafe resource id in container: " + resource.id;
      }
      return false;
    }
    if (resource.blobOffset + resource.size > container.resourcesBlob.size()) {
      if (outError != nullptr) {
        *outError = "resource blob out of range: " + resource.id;
      }
      return false;
    }

    std::vector<uint8_t> bytes(
        container.resourcesBlob.begin() +
            static_cast<std::ptrdiff_t>(resource.blobOffset),
        container.resourcesBlob.begin() +
            static_cast<std::ptrdiff_t>(resource.blobOffset + resource.size));
    if (crc32(bytes) != resource.crc32) {
      if (outError != nullptr) {
        *outError = "resource CRC mismatch: " + resource.id;
      }
      return false;
    }

    const fs::path outputPath = resourceRoot / fs::path(resource.id);
    fs::create_directories(outputPath.parent_path(), ec);
    if (ec) {
      if (outError != nullptr) {
        *outError = "failed to create resource path: " + ec.message();
      }
      return false;
    }

    std::ofstream out(outputPath, std::ios::binary | std::ios::trunc);
    if (!out.is_open()) {
      if (outError != nullptr) {
        *outError = "failed to write resource: " + outputPath.string();
      }
      return false;
    }
    if (!bytes.empty()) {
      out.write(reinterpret_cast<const char *>(bytes.data()),
                static_cast<std::streamsize>(bytes.size()));
    }
    fs::permissions(outputPath,
                    fs::perms::owner_write | fs::perms::group_write |
                        fs::perms::others_write,
                    fs::perm_options::remove, ec);
    sandbox->mountedResources[resource.id] = outputPath;
  }

  return true;
}

bool loadContainerForExecution(const fs::path &inputPath,
                               ContainerData *outContainer,
                               NconPermissionConfig *outPermissions,
                               std::string *outError) {
  if (outContainer == nullptr || outPermissions == nullptr) {
    if (outError != nullptr) {
      *outError = "internal error: null container execution output";
    }
    return false;
  }

  if (!readContainer(inputPath, outContainer, outError)) {
    return false;
  }
  if (!parseManifestPermissions(outContainer->manifestJson, outPermissions,
                                outError)) {
    return false;
  }
  return true;
}

fs::path watchCommandPath(const fs::path &sessionDir) {
  return sessionDir / "watch.command";
}

fs::path watchResponsePath(const fs::path &sessionDir) {
  return sessionDir / "watch.response";
}

bool parseWatchCommand(const fs::path &sessionDir, std::uint64_t *outSequence,
                       std::string *outType, fs::path *outContainerPath) {
  if (outSequence == nullptr || outType == nullptr || outContainerPath == nullptr) {
    return false;
  }

  const fs::path commandPath = watchCommandPath(sessionDir);
  if (!fs::exists(commandPath)) {
    return false;
  }

  std::ifstream in(commandPath, std::ios::binary);
  if (!in.is_open()) {
    return false;
  }

  std::string line;
  std::getline(in, line);
  if (line.empty()) {
    return false;
  }

  std::istringstream parts(line);
  std::uint64_t sequence = 0;
  std::string type;
  if (!(parts >> sequence >> type)) {
    return false;
  }

  std::string pathText;
  std::getline(parts, pathText);
  if (!pathText.empty() && pathText.front() == ' ') {
    pathText.erase(pathText.begin());
  }
  while (!pathText.empty() &&
         (pathText.back() == '\r' || pathText.back() == '\n' ||
          pathText.back() == ' ' || pathText.back() == '\t')) {
    pathText.pop_back();
  }

  *outSequence = sequence;
  *outType = type;
  *outContainerPath = pathText;
  return true;
}

bool writeWatchResponse(const fs::path &sessionDir, std::uint64_t sequence,
                        const std::string &status, const std::string &message,
                        std::string *outError) {
  std::ofstream out(watchResponsePath(sessionDir), std::ios::binary | std::ios::trunc);
  if (!out.is_open()) {
    if (outError != nullptr) {
      *outError = "failed to open watch response file";
    }
    return false;
  }
  out << sequence << ' ' << status;
  if (!message.empty()) {
    out << ' ' << message;
  }
  out << '\n';
  return out.good();
}

#ifdef _WIN32
bool resolveCurrentExecutablePath(const char *invokerPath, fs::path *outPath,
                                  std::string *outError) {
  if (outPath == nullptr) {
    if (outError != nullptr) {
      *outError = "internal error: null runner path output";
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
    fs::path fallback(invokerPath);
    std::error_code ec;
    if (!fallback.is_absolute()) {
      fallback = fs::absolute(fallback, ec);
      if (ec) {
        fallback.clear();
      }
    }
    if (!fallback.empty() && fs::exists(fallback, ec) && !ec) {
      *outPath = fallback;
      return true;
    }
  }

  if (outError != nullptr) {
    *outError = "failed to resolve current executable path for sandbox launch";
  }
  return false;
}
#endif

} // namespace

int runContainerDirect(const fs::path &inputPath) {
  ContainerData container;
  NconPermissionConfig permissions;
  std::string error;
  if (!loadContainerForExecution(inputPath, &container, &permissions, &error)) {
    std::cerr << error << std::endl;
    return 1;
  }

  SandboxContext sandbox;
  if (!initializeSandbox(containerHashForPath(inputPath), permissions, &sandbox,
                         &error) ||
      !materializeResources(container, &sandbox, &error)) {
    std::cerr << error << std::endl;
    return 1;
  }

  VM vm(container, sandbox);
  if (!vm.run(&error)) {
    std::cerr << error << std::endl;
    return 1;
  }
  return 0;
}

int runWatchSessionDirect(const fs::path &sessionDir, const fs::path &inputPath) {
  ContainerData container;
  NconPermissionConfig permissions;
  std::string error;
  if (!loadContainerForExecution(inputPath, &container, &permissions, &error)) {
    std::cerr << error << std::endl;
    return 1;
  }

  SandboxContext sandbox;
  if (!initializeSandbox(containerHashForPath(inputPath) + "-watch", permissions,
                         &sandbox, &error) ||
      !materializeResources(container, &sandbox, &error)) {
    std::cerr << error << std::endl;
    return 1;
  }

  VM vm(container, sandbox);
  std::uint64_t lastObservedSequence = 0;
  auto poller = [&]() -> std::optional<HotReloadCommand> {
    std::uint64_t sequence = 0;
    std::string type;
    fs::path nextContainerPath;
    if (!parseWatchCommand(sessionDir, &sequence, &type, &nextContainerPath)) {
      return std::nullopt;
    }
    if (sequence <= lastObservedSequence) {
      return std::nullopt;
    }
    lastObservedSequence = sequence;
    if (type != "patch") {
      return std::nullopt;
    }
    return HotReloadCommand{nextContainerPath, sequence};
  };
  auto reporter = [&](const HotReloadCommand &command,
                      const HotReloadResult &result) {
    const char *status = "failed";
    if (result.status == HotReloadResult::Status::Applied) {
      status = "applied";
    } else if (result.status == HotReloadResult::Status::RestartRequired) {
      status = "restart_required";
    }
    std::string responseError;
    writeWatchResponse(sessionDir, command.sequence, status, result.message,
                       &responseError);
  };

  if (!vm.runHotReloadSession(poller, reporter, &error)) {
    std::cerr << error << std::endl;
    return 1;
  }
  return 0;
}

int runContainerCommand(const fs::path &inputPath, const char *invokerPath) {
#ifdef _WIN32
  ContainerData container;
  NconPermissionConfig permissions;
  std::string error;
  if (!loadContainerForExecution(inputPath, &container, &permissions, &error)) {
    std::cerr << error << std::endl;
    return 1;
  }

  fs::path runnerPath;
  if (!resolveCurrentExecutablePath(invokerPath, &runnerPath, &error)) {
    std::cerr << error << std::endl;
    return 1;
  }
  int exitCode = 1;
  if (!executeContainerInWindowsSandbox(
          runnerPath, inputPath, containerHashForPath(inputPath), permissions,
          &exitCode, &error)) {
    std::cerr << error << std::endl;
    return 1;
  }
  return exitCode;
#else
  (void)invokerPath;
  return runContainerDirect(inputPath);
#endif
}

} // namespace neuron::ncon
