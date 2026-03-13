// RuntimePaths.cpp â€” Runtime dizin ve platform yardÄ±mcÄ±larÄ±nÄ±n implementasyonu.
// Bkz. RuntimePaths.h
#include "RuntimePaths.h"
#include "AppGlobals.h"
#include "ToolchainUtils.h"

#include "neuronc/cli/ModuleCppSupport.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

// â”€â”€ Directory helpers â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€

fs::path defaultRuntimeObjectCacheDir() {
  const char *overridePath = std::getenv("NEURON_RUNTIME_CACHE_DIR");
  if (overridePath != nullptr && *overridePath != '\0') {
    return fs::path(overridePath);
  }

#ifdef _WIN32
  const char *localAppData = std::getenv("LOCALAPPDATA");
  if (localAppData != nullptr && *localAppData != '\0') {
    return fs::path(localAppData) / "Neuron" / "runtime";
  }
#else
  const char *xdgCache = std::getenv("XDG_CACHE_HOME");
  if (xdgCache != nullptr && *xdgCache != '\0') {
    return fs::path(xdgCache) / "Neuron" / "runtime";
  }
  const char *home = std::getenv("HOME");
  if (home != nullptr && *home != '\0') {
    return fs::path(home) / ".cache" / "Neuron" / "runtime";
  }
#endif

  return g_toolRoot / "runtime";
}

bool canWriteToDirectory(const fs::path &dir) {
  std::error_code ec;
  fs::create_directories(dir, ec);
  if (ec) {
    return false;
  }

  const fs::path probeFile = dir / ".write_test";
  std::ofstream out(probeFile.string(), std::ios::out | std::ios::trunc);
  if (!out.is_open()) {
    return false;
  }
  out << "ok";
  out.close();
  fs::remove(probeFile, ec);
  return true;
}

fs::path runtimeObjectDirectory() {
  if (!g_runtimeObjectDir.empty()) {
    return g_runtimeObjectDir;
  }

  const fs::path localRuntimeDir = g_toolRoot / "runtime";
  if (canWriteToDirectory(localRuntimeDir)) {
    g_runtimeObjectDir = localRuntimeDir;
    return g_runtimeObjectDir;
  }

  const fs::path cacheDir = defaultRuntimeObjectCacheDir();
  if (canWriteToDirectory(cacheDir)) {
    g_runtimeObjectDir = cacheDir;
    return g_runtimeObjectDir;
  }

  g_runtimeObjectDir = localRuntimeDir;
  return g_runtimeObjectDir;
}

// â”€â”€ Platform detection â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€

std::string currentHostPlatform() {
#if defined(_WIN32) && (defined(_M_X64) || defined(__x86_64__))
  return "windows_x64";
#elif defined(__linux__) && defined(__x86_64__)
  return "linux_x64";
#elif defined(__APPLE__) && defined(__aarch64__)
  return "macos_arm64";
#else
  return "unsupported";
#endif
}

// â”€â”€ Artifact discovery â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€

std::vector<fs::path> candidateLibraryNames(const std::string &targetName) {
  if (targetName.empty()) {
    return {};
  }

  std::vector<fs::path> candidates;
#if defined(_WIN32)
  candidates.push_back(targetName + ".dll");
  candidates.push_back("lib" + targetName + ".dll");
#elif defined(__APPLE__)
  candidates.push_back("lib" + targetName + ".dylib");
  candidates.push_back(targetName + ".dylib");
#else
  candidates.push_back("lib" + targetName + ".so");
  candidates.push_back(targetName + ".so");
#endif
  return candidates;
}

bool findBuiltNativeArtifact(const fs::path &buildDir,
                             const std::string &targetName,
                             fs::path *outArtifact, std::string *outError) {
  if (outArtifact == nullptr) {
    if (outError != nullptr) {
      *outError = "internal error: null native artifact output";
    }
    return false;
  }

  const auto candidates = candidateLibraryNames(targetName);
  for (const auto &entry : fs::recursive_directory_iterator(buildDir)) {
    if (!entry.is_regular_file()) {
      continue;
    }
    for (const auto &candidate : candidates) {
      if (entry.path().filename() == candidate) {
        *outArtifact = entry.path();
        return true;
      }
    }
  }

  if (outError != nullptr) {
    *outError = "failed to locate built native artifact for target '" +
                targetName + "' in " + buildDir.string();
  }
  return false;
}

// â”€â”€ ModuleCpp compilation â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€

bool buildModuleCppFromSource(const fs::path &projectRoot,
                              const neuron::LoadedModuleCppModule &module,
                              const std::string &hostPlatform,
                              fs::path *outArtifact, std::string *outError) {
  if (outArtifact == nullptr) {
    if (outError != nullptr) {
      *outError = "internal error: null native build artifact output";
    }
    return false;
  }
  if (module.config.sourceDir.empty()) {
    if (outError != nullptr) {
      *outError = "modulecpp '" + module.name + "' has no source_dir";
    }
    return false;
  }

  const std::string buildSystem =
      module.config.buildSystem.empty() ? "cmake" : module.config.buildSystem;
  if (buildSystem != "cmake") {
    if (outError != nullptr) {
      *outError = "modulecpp '" + module.name +
                  "' uses unsupported build_system: " + buildSystem;
    }
    return false;
  }
  if (module.config.cmakeTarget.empty()) {
    if (outError != nullptr) {
      *outError = "modulecpp '" + module.name +
                  "' requires cmake_target when source_dir is set";
    }
    return false;
  }

  const fs::path sourceDir =
      (projectRoot / module.config.sourceDir).lexically_normal();
  const fs::path buildDir =
      projectRoot / "build" / "modulecpp" / module.name / hostPlatform;
  std::error_code ec;
  fs::create_directories(buildDir, ec);
  if (ec) {
    if (outError != nullptr) {
      *outError = "failed to create native build directory '" +
                  buildDir.string() + "': " + ec.message();
    }
    return false;
  }

  const std::string configureCommand =
      "cmake -S " + quotePath(sourceDir) + " -B " + quotePath(buildDir);
  if (runSystemCommand(configureCommand) != 0) {
    if (outError != nullptr) {
      *outError = "failed to configure modulecpp '" + module.name + "'";
    }
    return false;
  }

  std::string buildCommand = "cmake --build " + quotePath(buildDir) +
                             " --target " + module.config.cmakeTarget;
#if defined(_WIN32)
  buildCommand += " --config Release";
#endif
  if (runSystemCommand(buildCommand) != 0) {
    if (outError != nullptr) {
      *outError = "failed to build modulecpp '" + module.name + "'";
    }
    return false;
  }

  return findBuiltNativeArtifact(buildDir, module.config.cmakeTarget,
                                 outArtifact, outError);
}
