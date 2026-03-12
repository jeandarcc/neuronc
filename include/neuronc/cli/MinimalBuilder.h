#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace neuron {

enum class MinimalTargetPlatform {
  WindowsX64,
  LinuxX64,
  MacosArm64,
  Unknown,
};

struct MinimalBuildOptions {
  MinimalTargetPlatform platform = MinimalTargetPlatform::Unknown;
  std::string platformId;
  std::string compilerPath;
  std::filesystem::path outputPath;
  std::filesystem::path extraObjPath;
  bool compilerProvided = false;
  bool outputProvided = false;
  bool verbose = false;
};

bool parseMinimalTargetPlatform(const std::string &rawPlatform,
                                MinimalTargetPlatform *outPlatform);
std::string minimalTargetPlatformId(MinimalTargetPlatform platform);
std::string minimalDefaultOutputName(MinimalTargetPlatform platform);
bool minimalIsHostPlatform(MinimalTargetPlatform platform,
                           const std::string &hostPlatformId);

bool parseMinimalBuildArgs(const std::vector<std::string> &args,
                           const std::string &hostPlatformId,
                           MinimalBuildOptions *outOptions,
                           std::string *outError);

std::string buildMinimalCompileCommand(const std::string &compilerPath,
                                       const std::filesystem::path &sourcePath,
                                       const std::filesystem::path &objectPath,
                                       const std::filesystem::path &toolRoot);

std::string
buildMinimalLinkCommand(const std::string &compilerPath,
                        const std::vector<std::filesystem::path> &objectPaths,
                        const std::filesystem::path &outputPath,
                        const std::string &platformId);

bool writeMinimalLinkResponseFile(
    const std::filesystem::path &responseFilePath,
    const std::vector<std::filesystem::path> &objectPaths,
    std::string *outError);

std::string buildMinimalLinkCommandWithResponseFile(
    const std::string &compilerPath,
    const std::filesystem::path &responseFilePath,
    const std::filesystem::path &outputPath, const std::string &platformId);

} // namespace neuron
