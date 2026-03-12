// ToolchainUtils.h — Toolchain path resolution and system command helpers.
//
// This module provides the following operations:
//   - Path quoting and tool name resolution
//   - System command execution (with PATH + toolchain override support)
//   - File/DLL copy tools
//   - Toolchain root directory and runtime source existence detection
//
// If you want to add a new toolchain helper, add it here and
// write its implementation in ToolchainUtils.cpp.
#pragma once

#include <filesystem>
#include <optional>
#include <string>

namespace fs = std::filesystem;

// ── Path helpers ────────────────────────────────────────────────────────────

/// Wraps a path in double-quotes (for paths containing spaces).
std::string quotePath(const fs::path &path);

/// Resolves tool name to full path relative to g_toolchainBinDir (e.g. "gcc" →
/// full path).
std::string resolveToolPath(const std::string &toolName);

/// Returns the result of resolveToolPath directly as a command (quoted full
/// path or just tool name).
std::string resolveToolCommand(const std::string &toolName);

// ── System command ──────────────────────────────────────────────────────────

/// Runs a system command. On Windows, g_toolchainBinDir is added to PATH.
int runSystemCommand(const std::string &command);

// ── File copying ────────────────────────────────────────────────────────────

/// Copies source file to destination if it exists (creates destination if
/// needed). Returns success silently if source does not exist.
bool copyFileIfExists(const fs::path &source, const fs::path &destination,
                      std::string *outError = nullptr);

/// Copies GCC runtime DLLs from g_toolchainBinDir to destination directory on
/// Windows. If includeOpenMP=true, libgomp is also copied.
bool copyBundledRuntimeDlls(const fs::path &destinationDir, bool includeOpenMP,
                            std::string *outError = nullptr);

/// Copies all .dll files in the source directory to the destination directory.
bool copyOutputDllsToDirectory(const fs::path &sourceDir,
                               const fs::path &destinationDir,
                               std::string *outError = nullptr);

// ── Toolchain root detection ────────────────────────────────────────────────

/// Checks if runtime source files exist in the given directory.
bool hasRuntimeSourcesAt(const fs::path &root);

/// Checks if the specified tool executable exists in the given directory.
bool hasToolExecutableAt(const fs::path &dir, const std::string &toolName);

/// Estimates g_toolRoot by climbing up from the executable path.
std::optional<fs::path> findToolRootFromExecutable(const fs::path &exePath);

/// Initializes g_toolchainBinDir based on NEURON_TOOLCHAIN_BIN env or bundled
/// path.
void initializeToolchainBinDir();
