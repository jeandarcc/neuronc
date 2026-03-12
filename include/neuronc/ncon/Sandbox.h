#pragma once

#include "neuronc/ncon/Permissions.h"

#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

namespace neuron::ncon {

enum class SandboxAccessMode {
  Read,
  Write,
};

struct SandboxContext {
  std::filesystem::path workDirectory;
  std::filesystem::path resourceDirectory;
  std::filesystem::path nativeCacheDirectory;
  std::unordered_map<std::string, std::filesystem::path> mountedResources;
  std::vector<std::string> fsReadAllowList;
  std::vector<std::string> fsWriteAllowList;
  bool networkAllowed = false;
  bool processSpawnAllowed = false;
};

bool initializeSandbox(const std::string &containerHash,
                       const neuron::NconPermissionConfig &permissions,
                       SandboxContext *outContext, std::string *outError);

bool isLogicalPathAllowed(const SandboxContext &context,
                          const std::string &logicalPath,
                          SandboxAccessMode mode, std::string *outError);

bool resolveLogicalPath(const SandboxContext &context,
                        const std::string &logicalPath,
                        SandboxAccessMode mode,
                        std::filesystem::path *outPath,
                        std::string *outError);

bool resolveResourcePath(const SandboxContext &context,
                         const std::string &resourceId,
                         std::filesystem::path *outPath,
                         std::string *outError);

bool stageContainerForSandbox(const std::filesystem::path &sourceContainerPath,
                              const SandboxContext &context,
                              std::filesystem::path *outStagedContainerPath,
                              std::string *outError);

bool launchSandboxedProcess(const std::filesystem::path &runnerPath,
                            const std::vector<std::string> &arguments,
                            const SandboxContext &context, int *outExitCode,
                            std::string *outError);

bool executeContainerInWindowsSandbox(
    const std::filesystem::path &runnerPath,
    const std::filesystem::path &sourceContainerPath,
    const std::string &containerHash,
    const neuron::NconPermissionConfig &permissions, int *outExitCode,
    std::string *outError);

} // namespace neuron::ncon
