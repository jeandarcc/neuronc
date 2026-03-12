// ToolchainUtils.cpp — Araç zinciri yardımcılarının implementasyonu.
// Bkz. ToolchainUtils.h
#include "ToolchainUtils.h"
#include "AppGlobals.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#endif

// ── Path helpers ────────────────────────────────────────────────────────────

std::string quotePath(const fs::path &path) {
  return "\"" + path.string() + "\"";
}

std::string resolveToolPath(const std::string &toolName) {
  if (!g_toolchainBinDir.empty()) {
#ifdef _WIN32
    fs::path candidate = fs::path(g_toolchainBinDir) / (toolName + ".exe");
#else
    fs::path candidate = fs::path(g_toolchainBinDir) / toolName;
#endif
    candidate.make_preferred();
    if (fs::exists(candidate)) {
      return candidate.string();
    }
  }
  return toolName;
}

std::string resolveToolCommand(const std::string &toolName) {
  const std::string resolvedPath = resolveToolPath(toolName);
  if (resolvedPath == toolName) {
    return toolName;
  }
  return quotePath(resolvedPath);
}

// ── System command ──────────────────────────────────────────────────────────

int runSystemCommand(const std::string &command) {
#ifdef _WIN32
  if (!g_toolchainBinDir.empty()) {
    fs::path toolchainPath(g_toolchainBinDir);
    toolchainPath.make_preferred();
    std::string wrapped =
        "set \"PATH=" + toolchainPath.string() + ";%PATH%\" && " + command;
    return system(wrapped.c_str());
  }
#endif
  return system(command.c_str());
}

// ── File copying ────────────────────────────────────────────────────────────

bool copyFileIfExists(const fs::path &source, const fs::path &destination,
                      std::string *outError) {
  if (!fs::exists(source)) {
    return true;
  }

  std::error_code ec;
  fs::create_directories(destination.parent_path(), ec);
  if (ec) {
    if (outError != nullptr) {
      *outError = "Failed to create directory '" +
                  destination.parent_path().string() + "': " + ec.message();
    }
    return false;
  }

  fs::copy_file(source, destination, fs::copy_options::overwrite_existing, ec);
  if (ec) {
    if (outError != nullptr) {
      *outError = "Failed to copy '" + source.string() + "' to '" +
                  destination.string() + "': " + ec.message();
    }
    return false;
  }

  return true;
}

bool copyBundledRuntimeDlls(const fs::path &destinationDir, bool includeOpenMP,
                            std::string *outError) {
#ifdef _WIN32
  if (g_toolchainBinDir.empty()) {
    return true;
  }

  const fs::path toolchainBinDir(g_toolchainBinDir);
  const std::vector<std::string> exactDlls = {
      "libstdc++-6.dll", "libwinpthread-1.dll", "libquadmath-0.dll",
      "libatomic-1.dll", "libssp-0.dll"};

  for (const auto &dllName : exactDlls) {
    if (!copyFileIfExists(toolchainBinDir / dllName, destinationDir / dllName,
                          outError)) {
      return false;
    }
  }

  for (const auto &entry : fs::directory_iterator(toolchainBinDir)) {
    if (!entry.is_regular_file()) {
      continue;
    }
    const std::string fileName = entry.path().filename().string();
    const bool isLibGcc = fileName.rfind("libgcc_s_", 0) == 0 &&
                          entry.path().extension() == ".dll";
    const bool isLibGomp = includeOpenMP && fileName == "libgomp-1.dll";
    const bool isLibFfi = fileName.rfind("libffi-", 0) == 0 &&
                          entry.path().extension() == ".dll";
    if (!isLibGcc && !isLibGomp && !isLibFfi) {
      continue;
    }
    if (!copyFileIfExists(entry.path(),
                          destinationDir / entry.path().filename(), outError)) {
      return false;
    }
  }
#else
  (void)destinationDir;
  (void)includeOpenMP;
  (void)outError;
#endif
  return true;
}

bool copyOutputDllsToDirectory(const fs::path &sourceDir,
                               const fs::path &destinationDir,
                               std::string *outError) {
  std::error_code ec;
  fs::create_directories(destinationDir, ec);
  if (ec) {
    if (outError != nullptr) {
      *outError = "Failed to create release directory '" +
                  destinationDir.string() + "': " + ec.message();
    }
    return false;
  }

  for (const auto &entry : fs::directory_iterator(sourceDir, ec)) {
    if (ec) {
      if (outError != nullptr) {
        *outError = "Failed to enumerate output directory '" +
                    sourceDir.string() + "': " + ec.message();
      }
      return false;
    }
    if (!entry.is_regular_file() || entry.path().extension() != ".dll") {
      continue;
    }
    if (!copyFileIfExists(entry.path(),
                          destinationDir / entry.path().filename(), outError)) {
      return false;
    }
  }

  return true;
}

// ── Toolchain root detection ────────────────────────────────────────────────

bool hasRuntimeSourcesAt(const fs::path &root) {
  return fs::exists(root / "runtime/src/runtime.c") &&
         fs::exists(root / "runtime/include/neuron_runtime.h");
}

bool hasToolExecutableAt(const fs::path &dir, const std::string &toolName) {
#ifdef _WIN32
  return fs::exists(dir / (toolName + ".exe"));
#else
  return fs::exists(dir / toolName);
#endif
}

std::optional<fs::path> findToolRootFromExecutable(const fs::path &exePath) {
  if (exePath.empty()) {
    return std::nullopt;
  }

  fs::path probe = exePath.parent_path();
  for (int depth = 0; depth < 6; ++depth) {
    if (hasRuntimeSourcesAt(probe)) {
      return probe;
    }
    const fs::path parent = probe.parent_path();
    if (parent == probe) {
      break;
    }
    probe = parent;
  }
  return std::nullopt;
}

void initializeToolchainBinDir() {
  const char *overridePath = std::getenv("NEURON_TOOLCHAIN_BIN");
  if (overridePath != nullptr && *overridePath != '\0') {
    const fs::path overrideBinDir(overridePath);
    if (hasToolExecutableAt(overrideBinDir, "gcc")) {
      g_toolchainBinDir = overrideBinDir.string();
      return;
    }
  }

  const fs::path bundledBinDir = g_toolRoot / "toolchain/bin";
  if (hasToolExecutableAt(bundledBinDir, "gcc")) {
    g_toolchainBinDir = bundledBinDir.string();
    return;
  }

  if (!g_toolchainBinDir.empty()) {
    const fs::path compiledBinDir(g_toolchainBinDir);
    if (hasToolExecutableAt(compiledBinDir, "gcc")) {
      g_toolchainBinDir = compiledBinDir.string();
      return;
    }
  }

  g_toolchainBinDir.clear();
}
