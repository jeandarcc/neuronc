#include "neuronc/ncon/NconMiniCLI.h"

#include "neuronc/ncon/Runner.h"

#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

namespace neuron::ncon {

namespace {

static constexpr char kEmbedMagic[8] = {'N', 'C', 'O', 'N', 'P', 'R', 'O', 'D'};
static constexpr size_t kFooterSize = 16;

void printUsage(const char *programName) {
  std::cout << "Usage:\n"
            << "  " << programName << " <program.ncon>\n";
}

std::filesystem::path getExecutablePath(const char *argv0) {
#ifdef _WIN32
  char buf[MAX_PATH];
  DWORD len = GetModuleFileNameA(nullptr, buf, MAX_PATH);
  if (len > 0 && len < MAX_PATH) {
    return std::filesystem::path(buf);
  }
#else
  char buf[4096];
  ssize_t len = readlink("/proc/self/exe", buf, sizeof(buf) - 1);
  if (len > 0) {
    buf[len] = '\0';
    return std::filesystem::path(buf);
  }
#endif
  // Fallback to argv[0]
  if (argv0 != nullptr) {
    return std::filesystem::path(argv0);
  }
  return {};
}

bool checkEmbeddedPayload(const std::filesystem::path &exePath) {
  std::ifstream file(exePath, std::ios::binary | std::ios::ate);
  if (!file.is_open()) {
    return false;
  }
  auto fileSize = static_cast<uint64_t>(file.tellg());
  if (fileSize < kFooterSize) {
    return false;
  }
  file.seekg(-static_cast<std::streamoff>(8), std::ios::end);
  char magic[8];
  file.read(magic, 8);
  return std::memcmp(magic, kEmbedMagic, 8) == 0;
}

bool extractEmbeddedToTemp(const std::filesystem::path &exePath,
                           std::filesystem::path *outTempPath) {
  std::ifstream file(exePath, std::ios::binary | std::ios::ate);
  if (!file.is_open()) {
    return false;
  }

  auto fileSize = static_cast<uint64_t>(file.tellg());
  if (fileSize < kFooterSize) {
    return false;
  }

  // Read footer
  file.seekg(-static_cast<std::streamoff>(kFooterSize), std::ios::end);
  uint64_t payloadSize = 0;
  file.read(reinterpret_cast<char *>(&payloadSize), 8);
  char magic[8];
  file.read(magic, 8);

  if (std::memcmp(magic, kEmbedMagic, 8) != 0 || payloadSize == 0 ||
      payloadSize + kFooterSize > fileSize) {
    return false;
  }

  // Read payload
  file.seekg(-static_cast<std::streamoff>(kFooterSize + payloadSize),
             std::ios::end);
  std::vector<char> payload(payloadSize);
  file.read(payload.data(), static_cast<std::streamsize>(payloadSize));
  file.close();

  // Write to temp file
  namespace fs = std::filesystem;
  fs::path tempDir = fs::temp_directory_path() / "neuron_product";
  fs::create_directories(tempDir);
  fs::path tempNcon = tempDir / "embedded.ncon";

  std::ofstream out(tempNcon, std::ios::binary);
  if (!out.is_open()) {
    return false;
  }
  out.write(payload.data(), static_cast<std::streamsize>(payloadSize));
  out.close();

  *outTempPath = tempNcon;
  return true;
}

} // namespace

MiniCliAction parseMiniCliAction(const std::vector<std::string> &args) {
  MiniCliAction action;
  if (args.empty()) {
    // Will be handled by embedded payload check in runMiniCli
    action.kind = MiniCliActionKind::RunEmbedded;
    return action;
  }

  const std::string first = args[0];
  if (first == "-h" || first == "--help" || first == "help") {
    action.kind = MiniCliActionKind::ShowHelp;
    return action;
  }

  if (first == "__sandbox_run") {
    if (args.size() != 2u) {
      action.kind = MiniCliActionKind::Error;
      action.error = "sandbox helper requires a single staged container path";
      return action;
    }
    action.kind = MiniCliActionKind::RunSandboxHelper;
    action.containerPath = args[1];
    return action;
  }

  if (first == "run" || first == "build" || first == "watch" ||
      first == "inspect" || first == "config") {
    action.kind = MiniCliActionKind::Error;
    action.error = "minimal runtime supports only direct execution: nucleus "
                   "<file.ncon>";
    return action;
  }

  if (args.size() != 1u) {
    action.kind = MiniCliActionKind::Error;
    action.error = "minimal runtime accepts a single .ncon input";
    return action;
  }

  action.kind = MiniCliActionKind::RunContainer;
  action.containerPath = first;
  return action;
}

int runMiniCli(int argc, char *argv[]) {
  std::vector<std::string> args;
  args.reserve(argc > 1 ? static_cast<size_t>(argc - 1) : 0u);
  for (int i = 1; i < argc; ++i) {
    args.emplace_back(argv[i]);
  }

  const MiniCliAction action = parseMiniCliAction(args);
  switch (action.kind) {
  case MiniCliActionKind::ShowHelp:
    printUsage(argc > 0 ? argv[0] : "nucleus");
    return 0;

  case MiniCliActionKind::RunEmbedded: {
    // Check for embedded NCON payload in own binary
    std::filesystem::path exePath =
        getExecutablePath(argc > 0 ? argv[0] : nullptr);
    if (!exePath.empty() && checkEmbeddedPayload(exePath)) {
      std::filesystem::path tempNcon;
      if (extractEmbeddedToTemp(exePath, &tempNcon)) {
        return runContainerDirect(tempNcon.string());
      }
      std::cerr << "Failed to extract embedded container" << std::endl;
      return 1;
    }
    // No embedded payload and no args → show help
    std::cerr << "missing input container path" << std::endl;
    printUsage(argc > 0 ? argv[0] : "nucleus");
    return 1;
  }

  case MiniCliActionKind::RunContainer:
    return runContainerCommand(action.containerPath,
                               argc > 0 ? argv[0] : nullptr);

  case MiniCliActionKind::RunSandboxHelper:
    return runContainerDirect(action.containerPath);

  case MiniCliActionKind::Error:
    if (!action.error.empty()) {
      std::cerr << action.error << std::endl;
    }
    printUsage(argc > 0 ? argv[0] : "nucleus");
    return 1;
  }

  std::cerr << "internal error: unsupported mini CLI action" << std::endl;
  return 1;
}

} // namespace neuron::ncon
