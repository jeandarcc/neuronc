#include "neuronc/cli/installer/InstallerBuilder.h"

#include <fstream>
#include <iostream>
#include <sstream>
#include <vector>

namespace neuron {

namespace {

std::string quoteStr(const std::filesystem::path &p) {
  return "\"" + p.string() + "\"";
}

std::string unquoteIfNeeded(const std::string &value) {
  if (value.size() >= 2 && value.front() == '"' && value.back() == '"') {
    return value.substr(1, value.size() - 2);
  }
  return value;
}

std::filesystem::path
resolveCompilerPath(const std::string &requestedCompiler,
                    const std::filesystem::path &toolRoot) {
  namespace fs = std::filesystem;

  const std::string rawCompiler = unquoteIfNeeded(requestedCompiler);
  const std::string compilerName = rawCompiler.empty() ? "g++" : rawCompiler;

  auto withOptionalExe = [](const fs::path &candidate) -> fs::path {
#ifdef _WIN32
    if (candidate.extension().empty()) {
      fs::path withExe = candidate;
      withExe += ".exe";
      if (fs::exists(withExe)) {
        return withExe;
      }
    }
#endif
    return candidate;
  };

  fs::path directCandidate(compilerName);
  directCandidate = withOptionalExe(directCandidate);
  if ((directCandidate.has_parent_path() || directCandidate.is_absolute()) &&
      fs::exists(directCandidate)) {
    return directCandidate;
  }

  std::vector<fs::path> candidates = {
      toolRoot / "toolchain" / "bin" / compilerName,
      toolRoot / "bin" / compilerName,
      fs::path("C:/msys64/mingw64/bin") / compilerName,
  };

  for (auto &candidate : candidates) {
    candidate = withOptionalExe(candidate);
    if (fs::exists(candidate)) {
      return candidate;
    }
  }

  // Fallback to PATH lookup.
  return fs::path(compilerName);
}

int runSystemCommand(const std::string &cmd, bool verbose,
                     const std::filesystem::path &pathPrefix = {}) {
  std::string effectiveCmd = cmd;
#ifdef _WIN32
  if (!pathPrefix.empty()) {
    effectiveCmd =
        "set \"PATH=" + pathPrefix.string() + ";%PATH%\" && " + effectiveCmd;
  }
#endif
  if (verbose) {
    std::cout << "[installer-builder] " << effectiveCmd << std::endl;
  }
  return std::system(effectiveCmd.c_str());
}

} // namespace

InstallerBuildResult
buildInstaller(const ProductSettings &settings,
               const ProductBuildOptions &options,
               const std::filesystem::path &productExePath,
               const std::filesystem::path &resourceObjPath,
               const std::filesystem::path &projectRoot,
               const std::filesystem::path &toolRoot) {

  InstallerBuildResult result;
  namespace fs = std::filesystem;

  if (options.platform != MinimalTargetPlatform::WindowsX64) {
    result.errors.push_back(
        "Installer generation currently only supported for Windows platform.");
    return result;
  }

  const std::string versionTag = settings.productVersion;
  const fs::path outputDir = projectRoot / settings.outputDir / versionTag;
  fs::create_directories(outputDir);

  const fs::path installerExe =
      outputDir / (settings.outputName + "-Setup.exe");
  const fs::path installerSrc =
      toolRoot / "runtime" / "installer" / "InstallerMain.cpp";

  if (!fs::exists(installerSrc)) {
    result.errors.push_back("Installer source not found: " +
                            installerSrc.string());
    return result;
  }

  const fs::path uninstallerSrc =
      toolRoot / "runtime" / "installer" / "UninstallerMain.cpp";
  const fs::path updaterSrc =
      toolRoot / "runtime" / "installer" / "UpdaterMain.cpp";
  const fs::path uninstallerExe =
      outputDir / (settings.outputName + "-Uninstall.exe");
  const fs::path updaterExe = outputDir / (settings.outputName + "-Updater.exe");

  // Compile installer exe using g++ directly since build-nucleus orchestrator
  // is geared towards Neuron++ linking
  std::cout << "  [i] Compiling installer executable..." << std::endl;
  const fs::path compilerPath = resolveCompilerPath(options.compilerPath, toolRoot);
  const fs::path compilerBinDir = compilerPath.has_parent_path()
                                      ? compilerPath.parent_path()
                                      : fs::path();
  if (options.verbose) {
    std::cout << "  [i] Using compiler: " << compilerPath.string() << std::endl;
  }

  std::ostringstream compileCmd;
  compileCmd << quoteStr(compilerPath);
  compileCmd << " -std=c++20 -O2";
  compileCmd << " " << quoteStr(installerSrc);
  if (!resourceObjPath.empty() && fs::exists(resourceObjPath)) {
    compileCmd << " " << quoteStr(resourceObjPath);
  }
  compileCmd << " -o " << quoteStr(installerExe);
  compileCmd << " -mwindows -static -static-libstdc++ -static-libgcc";
  compileCmd << " -luser32 -lshell32 -ladvapi32 -lgdi32 -lcomctl32 -lole32";

  if (runSystemCommand(compileCmd.str(), options.verbose, compilerBinDir) != 0) {
    result.errors.push_back("Failed to compile installer executable.");
    return result;
  }

  if (settings.uninstallerEnabled) {
    if (!fs::exists(uninstallerSrc)) {
      result.errors.push_back("Uninstaller source not found: " +
                              uninstallerSrc.string());
      return result;
    }
    std::cout << "  [i] Compiling uninstaller executable..." << std::endl;
    std::ostringstream cmd;
    cmd << quoteStr(compilerPath) << " -std=c++20 -O2 " << quoteStr(uninstallerSrc)
        << " -o " << quoteStr(uninstallerExe)
        << " -mwindows -static -static-libstdc++ -static-libgcc"
        << " -luser32 -lshell32 -ladvapi32";
    if (runSystemCommand(cmd.str(), options.verbose, compilerBinDir) != 0) {
      result.errors.push_back("Failed to compile uninstaller executable.");
      return result;
    }
  }

  if (settings.updateEnabled && !options.noUpdater) {
    if (!fs::exists(updaterSrc)) {
      result.errors.push_back("Updater source not found: " +
                              updaterSrc.string());
      return result;
    }
    std::cout << "  [i] Compiling updater executable..." << std::endl;
    std::ostringstream cmd;
    cmd << quoteStr(compilerPath) << " -std=c++20 -O2 " << quoteStr(updaterSrc)
        << " -o " << quoteStr(updaterExe)
        << " -mwindows -static -static-libstdc++ -static-libgcc"
        << " -lwinhttp -luser32 -lshell32 -ladvapi32 -lcomctl32"
        << " -lcrypto -lcrypt32 -lbcrypt -lws2_32 -liphlpapi -lgdi32";
    if (runSystemCommand(cmd.str(), options.verbose, compilerBinDir) != 0) {
      result.errors.push_back("Failed to compile updater executable.");
      return result;
    }
  }

  struct PayloadInfo {
    std::filesystem::path filePath;
    std::string fileName;
    uint64_t offset = 0;
    uint64_t size = 0;
  };

  PayloadInfo productPayload{productExePath, productExePath.filename().string()};
  PayloadInfo updaterPayload{updaterExe, updaterExe.filename().string()};
  PayloadInfo uninstallerPayload{uninstallerExe, uninstallerExe.filename().string()};

  std::cout << "  [i] Embedding payloads into installer..." << std::endl;
  std::ofstream outExe(installerExe, std::ios::binary | std::ios::app);
  if (!outExe.is_open()) {
    result.errors.push_back("Failed to open installer for payload embedding.");
    return result;
  }

  auto appendPayload = [&](PayloadInfo *payload) -> bool {
    if (payload == nullptr || payload->filePath.empty() ||
        !fs::exists(payload->filePath)) {
      return true;
    }

    payload->offset = static_cast<uint64_t>(outExe.tellp());
    std::ifstream in(payload->filePath, std::ios::binary);
    if (!in.is_open()) {
      result.errors.push_back("Failed to open payload: " +
                              payload->filePath.string());
      return false;
    }

    std::vector<char> buffer(1 << 20, 0);
    uint64_t written = 0;
    while (in) {
      in.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
      const std::streamsize readCount = in.gcount();
      if (readCount <= 0) {
        break;
      }
      outExe.write(buffer.data(), readCount);
      written += static_cast<uint64_t>(readCount);
    }
    if (!outExe.good()) {
      result.errors.push_back("Failed while writing payload: " +
                              payload->filePath.string());
      return false;
    }
    payload->size = written;
    return true;
  };

  const uint64_t payloadBaseOffset = static_cast<uint64_t>(outExe.tellp());
  if (!appendPayload(&productPayload)) {
    return result;
  }
  if (settings.updateEnabled && !options.noUpdater) {
    if (!appendPayload(&updaterPayload)) {
      return result;
    }
  }
  if (settings.uninstallerEnabled) {
    if (!appendPayload(&uninstallerPayload)) {
      return result;
    }
  }
  const uint64_t payloadBlobSize =
      static_cast<uint64_t>(outExe.tellp()) - payloadBaseOffset;

  auto jsonBool = [](bool value) { return value ? "true" : "false"; };
  std::string manifestJson = "{\n";
  manifestJson += "  \"product_name\": \"" + settings.productName + "\",\n";
  manifestJson += "  \"product_version\": \"" + settings.productVersion + "\",\n";
  manifestJson += "  \"publisher\": \"" + settings.productPublisher + "\",\n";
  manifestJson += "  \"default_dir\": \"" + settings.installDirectoryDefault + "\",\n";
  manifestJson += "  \"create_desktop_shortcut\": " +
                  std::string(jsonBool(settings.createDesktopShortcut)) + ",\n";
  manifestJson += "  \"create_start_menu\": " +
                  std::string(jsonBool(settings.createStartMenuEntry)) + ",\n";
  manifestJson += "  \"update_enabled\": " +
                  std::string(jsonBool(settings.updateEnabled && !options.noUpdater)) +
                  ",\n";
  manifestJson += "  \"update_url\": \"" + settings.updateUrl + "\",\n";
  manifestJson += "  \"update_channel\": \"" + settings.updateChannel + "\",\n";
  manifestJson += "  \"update_public_key\": \"" + settings.updatePublicKey + "\",\n";
  manifestJson += "  \"executable_name\": \"" + productPayload.fileName + "\",\n";
  manifestJson += "  \"product_offset\": " +
                  std::to_string(productPayload.offset - payloadBaseOffset) + ",\n";
  manifestJson += "  \"product_size\": " + std::to_string(productPayload.size) + ",\n";
  manifestJson += "  \"updater_executable\": \"" +
                  ((settings.updateEnabled && !options.noUpdater) ? updaterPayload.fileName
                                                                   : std::string()) +
                  "\",\n";
  manifestJson += "  \"updater_offset\": " +
                  std::to_string((settings.updateEnabled && !options.noUpdater)
                                     ? updaterPayload.offset - payloadBaseOffset
                                     : 0) +
                  ",\n";
  manifestJson += "  \"updater_size\": " +
                  std::to_string((settings.updateEnabled && !options.noUpdater)
                                     ? updaterPayload.size
                                     : 0) +
                  ",\n";
  manifestJson += "  \"uninstaller_executable\": \"" +
                  (settings.uninstallerEnabled ? uninstallerPayload.fileName
                                               : std::string()) +
                  "\",\n";
  manifestJson += "  \"uninstaller_offset\": " +
                  std::to_string(settings.uninstallerEnabled
                                     ? uninstallerPayload.offset - payloadBaseOffset
                                     : 0) +
                  ",\n";
  manifestJson += "  \"uninstaller_size\": " +
                  std::to_string(settings.uninstallerEnabled
                                     ? uninstallerPayload.size
                                     : 0) +
                  "\n";
  manifestJson += "}";

  outExe.write(manifestJson.c_str(), static_cast<std::streamsize>(manifestJson.size()));
  const uint64_t manifestSize = static_cast<uint64_t>(manifestJson.size());

  outExe.write(reinterpret_cast<const char *>(&manifestSize), sizeof(uint64_t));
  outExe.write(reinterpret_cast<const char *>(&payloadBlobSize), sizeof(uint64_t));
  const char magic[] = "NPPINST1";
  outExe.write(magic, 8);
  outExe.close();

  result.installerExePath = installerExe;
  return result;
}

} // namespace neuron
