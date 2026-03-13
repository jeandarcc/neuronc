#include "neuronc/cli/MinimalBuilder.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>
#include <utility>

namespace neuron {

namespace {

std::string toLowerCopy(const std::string &value) {
  std::string lowered = value;
  std::transform(
      lowered.begin(), lowered.end(), lowered.begin(),
      [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
  return lowered;
}

std::string quotePath(const std::filesystem::path &path) {
  return "\"" + path.string() + "\"";
}

std::string quoteValue(const std::string &value) { return "\"" + value + "\""; }

} // namespace

bool parseMinimalTargetPlatform(const std::string &rawPlatform,
                                MinimalTargetPlatform *outPlatform) {
  if (outPlatform == nullptr) {
    return false;
  }

  const std::string lowered = toLowerCopy(rawPlatform);
  if (lowered == "windows" || lowered == "win" || lowered == "win-x64" ||
      lowered == "windows-x64" || lowered == "windows_x64") {
    *outPlatform = MinimalTargetPlatform::WindowsX64;
    return true;
  }
  if (lowered == "linux" || lowered == "linux-x64" || lowered == "linux_x64") {
    *outPlatform = MinimalTargetPlatform::LinuxX64;
    return true;
  }
  if (lowered == "mac" || lowered == "macos" || lowered == "mac-arm64" ||
      lowered == "macos-arm64" || lowered == "macos_arm64") {
    *outPlatform = MinimalTargetPlatform::MacosArm64;
    return true;
  }

  *outPlatform = MinimalTargetPlatform::Unknown;
  return false;
}

std::string minimalTargetPlatformId(MinimalTargetPlatform platform) {
  switch (platform) {
  case MinimalTargetPlatform::WindowsX64:
    return "windows_x64";
  case MinimalTargetPlatform::LinuxX64:
    return "linux_x64";
  case MinimalTargetPlatform::MacosArm64:
    return "macos_arm64";
  case MinimalTargetPlatform::Unknown:
    break;
  }
  return "unsupported";
}

std::string minimalDefaultOutputName(MinimalTargetPlatform platform) {
  if (platform == MinimalTargetPlatform::WindowsX64) {
    return "nucleus.exe";
  }
  return "nucleus";
}

bool minimalIsHostPlatform(MinimalTargetPlatform platform,
                           const std::string &hostPlatformId) {
  return minimalTargetPlatformId(platform) == hostPlatformId;
}

bool parseMinimalBuildArgs(const std::vector<std::string> &args,
                           const std::string &hostPlatformId,
                           MinimalBuildOptions *outOptions,
                           std::string *outError) {
  if (outOptions == nullptr) {
    if (outError != nullptr) {
      *outError = "internal error: null minimal build options output";
    }
    return false;
  }

  MinimalBuildOptions options;
  bool platformProvided = false;

  for (size_t i = 0; i < args.size(); ++i) {
    const std::string &arg = args[i];
    if (arg == "--platform") {
      if (i + 1 >= args.size()) {
        if (outError != nullptr) {
          *outError = "missing value for --platform";
        }
        return false;
      }
      ++i;
      if (!parseMinimalTargetPlatform(args[i], &options.platform)) {
        if (outError != nullptr) {
          *outError = "unsupported --platform value: " + args[i];
        }
        return false;
      }
      options.platformId = minimalTargetPlatformId(options.platform);
      platformProvided = true;
      continue;
    }
    if (arg == "--compiler") {
      if (i + 1 >= args.size()) {
        if (outError != nullptr) {
          *outError = "missing value for --compiler";
        }
        return false;
      }
      ++i;
      options.compilerPath = args[i];
      options.compilerProvided = true;
      continue;
    }
    if (arg == "--output") {
      if (i + 1 >= args.size()) {
        if (outError != nullptr) {
          *outError = "missing value for --output";
        }
        return false;
      }
      ++i;
      options.outputPath = args[i];
      options.outputProvided = true;
      continue;
    }
    if (arg == "--extra-obj") {
      if (i + 1 >= args.size()) {
        if (outError != nullptr) {
          *outError = "missing value for --extra-obj";
        }
        return false;
      }
      ++i;
      options.extraObjPath = args[i];
      continue;
    }
    if (arg == "--verbose") {
      options.verbose = true;
      continue;
    }

    if (outError != nullptr) {
      *outError = "unknown argument: " + arg;
    }
    return false;
  }

  if (!platformProvided) {
    if (outError != nullptr) {
      *outError = "build-nucleus requires --platform <Windows|Linux|Mac>";
    }
    return false;
  }
  if (!minimalIsHostPlatform(options.platform, hostPlatformId) &&
      !options.compilerProvided) {
    if (outError != nullptr) {
      *outError =
          "non-host targets require --compiler <cross-g++ wrapper path>";
    }
    return false;
  }

  if (options.compilerPath.empty()) {
    options.compilerPath = "g++";
  }
  if (options.outputPath.empty()) {
    options.outputPath = minimalDefaultOutputName(options.platform);
  }

  *outOptions = std::move(options);
  return true;
}

std::string buildMinimalCompileCommand(const std::string &compilerPath,
                                       const std::filesystem::path &sourcePath,
                                       const std::filesystem::path &objectPath,
                                       const std::filesystem::path &toolRoot) {
  const std::string ext = toLowerCopy(sourcePath.extension().string());
  const bool cSource = ext == ".c";

  std::ostringstream cmd;
  cmd << quoteValue(compilerPath);
  if (cSource) {
    cmd << " -x c -std=c11";
  } else {
    cmd << " -x c++ -std=c++20";
  }
  cmd << " -c " << quotePath(sourcePath) << " -o " << quotePath(objectPath);
  cmd << " -O2 -ffunction-sections -fdata-sections";
  cmd << " -DNeuron_ENABLE_CUDA_BACKEND=1 -DNeuron_ENABLE_VULKAN_BACKEND=1";
  cmd << " -I" << quotePath(toolRoot / "include");
  cmd << " -I" << quotePath(toolRoot / "runtime" / "include");
  cmd << " -I" << quotePath(toolRoot / "runtime" / "src");
  return cmd.str();
}

std::string
buildMinimalLinkCommand(const std::string &compilerPath,
                        const std::vector<std::filesystem::path> &objectPaths,
                        const std::filesystem::path &outputPath,
                        const std::string &platformId) {
  std::ostringstream cmd;
  cmd << quoteValue(compilerPath) << " -o " << quotePath(outputPath);
  for (const auto &obj : objectPaths) {
    cmd << " " << quotePath(obj);
  }

  cmd << " -Wl,--gc-sections";
  cmd << " -static -static-libstdc++ -static-libgcc";
  cmd << " -lffi -lm";

  if (platformId == "windows_x64") {
    cmd << " -ladvapi32 -lshell32 -lole32 -luser32 -lgdi32"
        << " -lwindowscodecs -luuid -lkernel32 -lws2_32";
  } else {
    cmd << " -ldl -lpthread";
  }

  return cmd.str();
}

bool writeMinimalLinkResponseFile(
    const std::filesystem::path &responseFilePath,
    const std::vector<std::filesystem::path> &objectPaths,
    std::string *outError) {
  std::ofstream out(responseFilePath, std::ios::trunc);
  if (!out.is_open()) {
    if (outError != nullptr) {
      *outError =
          "failed to open minimal link response file: " + responseFilePath.string();
    }
    return false;
  }

  for (const auto &obj : objectPaths) {
    out << quotePath(obj) << '\n';
  }

  if (!out.good()) {
    if (outError != nullptr) {
      *outError = "failed to write minimal link response file: " +
                  responseFilePath.string();
    }
    return false;
  }

  return true;
}

std::string buildMinimalLinkCommandWithResponseFile(
    const std::string &compilerPath,
    const std::filesystem::path &responseFilePath,
    const std::filesystem::path &outputPath, const std::string &platformId) {
  std::ostringstream cmd;
  cmd << quoteValue(compilerPath) << " -o " << quotePath(outputPath) << " @"
      << responseFilePath.string();

  cmd << " -Wl,--gc-sections";
  cmd << " -static -static-libstdc++ -static-libgcc";
  cmd << " -lffi -lm";

  if (platformId == "windows_x64") {
    cmd << " -ladvapi32 -lshell32 -lole32 -luser32 -lgdi32"
        << " -lwindowscodecs -luuid -lkernel32 -lws2_32";
  } else {
    cmd << " -ldl -lpthread";
  }

  return cmd.str();
}

} // namespace neuron
