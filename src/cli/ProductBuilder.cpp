#include "neuronc/cli/ProductBuilder.h"

#include "neuronc/cli/ResourceCompiler.h"
#include "neuronc/cli/installer/InstallerBuilder.h"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <fstream>
#include <iostream>
#include <sstream>

namespace neuron {

namespace {

static constexpr char kEmbedMagic[8] = {'N', 'C', 'O', 'N', 'P', 'R', 'O', 'D'};
static constexpr size_t kFooterSize = 16; // 8 bytes size + 8 bytes magic

std::string toLower(const std::string &s) {
  std::string result = s;
  std::transform(
      result.begin(), result.end(), result.begin(),
      [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  return result;
}

std::string quote(const std::filesystem::path &p) {
  return "\"" + p.string() + "\"";
}

bool incrementPatchVersion(const std::string &version, std::string *outNext,
                           std::string *outError) {
  if (outNext == nullptr) {
    if (outError != nullptr) {
      *outError = "internal error: null version output";
    }
    return false;
  }

  std::vector<std::string> parts;
  std::string token;
  std::istringstream stream(version);
  while (std::getline(stream, token, '.')) {
    parts.push_back(token);
  }
  if (parts.size() != 3u) {
    if (outError != nullptr) {
      *outError = "expected semantic version in form x.y.z";
    }
    return false;
  }

  auto parsePart = [](const std::string &part, int *out) -> bool {
    if (out == nullptr || part.empty()) {
      return false;
    }
    for (char ch : part) {
      if (ch < '0' || ch > '9') {
        return false;
      }
    }
    try {
      *out = std::stoi(part);
      return *out >= 0;
    } catch (...) {
      return false;
    }
  };

  int major = 0;
  int minor = 0;
  int patch = 0;
  if (!parsePart(parts[0], &major) || !parsePart(parts[1], &minor) ||
      !parsePart(parts[2], &patch)) {
    if (outError != nullptr) {
      *outError = "version must contain only numeric x.y.z segments";
    }
    return false;
  }

  ++patch;
  *outNext = std::to_string(major) + "." + std::to_string(minor) + "." +
             std::to_string(patch);
  return true;
}

std::string resolveNeuronCliCommand(const std::filesystem::path &toolRoot) {
  namespace fs = std::filesystem;

  std::vector<fs::path> candidates = {
      toolRoot / "neuronc",
      toolRoot / "neuron",
      toolRoot / "bin" / "neuronc",
      toolRoot / "bin" / "neuron",
      toolRoot / "build" / "bin" / "neuronc",
      toolRoot / "build" / "bin" / "neuron",
  };

#ifdef _WIN32
  for (const auto &candidate : candidates) {
    fs::path exeCandidate = candidate;
    exeCandidate.replace_extension(".exe");
    if (fs::exists(exeCandidate)) {
      return quote(exeCandidate);
    }
    if (fs::exists(candidate)) {
      return quote(candidate);
    }
  }
#else
  for (const auto &candidate : candidates) {
    if (fs::exists(candidate)) {
      return quote(candidate);
    }
  }
#endif

  // Fallback to PATH resolution.
  return "neuron";
}

int runCmd(const std::string &cmd, bool verbose) {
  std::string effectiveCmd = cmd;
#ifdef _WIN32
  // `system()` goes through cmd.exe; commands that begin with a quoted path
  // can fail to parse unless dispatched via `call`.
  if (!effectiveCmd.empty() && effectiveCmd.front() == '"') {
    effectiveCmd = "call " + effectiveCmd;
  }
#endif

  if (verbose) {
    std::cout << "[build-product] " << effectiveCmd << std::endl;
  }
  return std::system(effectiveCmd.c_str());
}

} // namespace

// ─────────────────────────────────────────────
// CLI argument parsing
// ─────────────────────────────────────────────

bool parseProductBuildArgs(const std::vector<std::string> &args,
                           const std::string &hostPlatformId,
                           ProductBuildOptions *outOptions,
                           std::string *outError) {
  if (outOptions == nullptr) {
    if (outError)
      *outError = "internal error: null options output";
    return false;
  }

  ProductBuildOptions options;
  bool platformProvided = false;

  for (size_t i = 0; i < args.size(); ++i) {
    const std::string &arg = args[i];
    if (arg == "--platform") {
      if (i + 1 >= args.size()) {
        if (outError)
          *outError = "missing value for --platform";
        return false;
      }
      ++i;
      if (!parseMinimalTargetPlatform(args[i], &options.platform)) {
        if (outError)
          *outError = "unsupported --platform value: " + args[i];
        return false;
      }
      options.platformId = minimalTargetPlatformId(options.platform);
      platformProvided = true;
    } else if (arg == "--compiler") {
      if (i + 1 >= args.size()) {
        if (outError)
          *outError = "missing value for --compiler";
        return false;
      }
      ++i;
      options.compilerPath = args[i];
    } else if (arg == "--no-installer") {
      options.noInstaller = true;
    } else if (arg == "--no-updater") {
      options.noUpdater = true;
    } else if (arg == "--verbose") {
      options.verbose = true;
    } else {
      if (outError)
        *outError = "unknown argument: " + arg;
      return false;
    }
  }

  if (!platformProvided) {
    if (outError)
      *outError = "build-product requires --platform <Windows|Linux|Mac>";
    return false;
  }

  if (options.compilerPath.empty()) {
    options.compilerPath = "g++";
  }

  *outOptions = std::move(options);
  return true;
}

// ─────────────────────────────────────────────
// NCON Embedding (self-extracting pattern)
// ─────────────────────────────────────────────

bool embedNconInBinary(const std::filesystem::path &binaryPath,
                       const std::filesystem::path &nconPath,
                       std::string *outError) {
  // Read the NCON container
  std::ifstream nconFile(nconPath, std::ios::binary | std::ios::ate);
  if (!nconFile.is_open()) {
    if (outError)
      *outError = "Cannot open NCON container: " + nconPath.string();
    return false;
  }

  auto nconSize = static_cast<uint64_t>(nconFile.tellg());
  nconFile.seekg(0, std::ios::beg);

  std::vector<char> nconData(nconSize);
  if (!nconFile.read(nconData.data(), static_cast<std::streamsize>(nconSize))) {
    if (outError)
      *outError = "Failed to read NCON container";
    return false;
  }
  nconFile.close();

  // Append to binary: [ncon data] [8 bytes size] [8 bytes magic]
  std::ofstream binary(binaryPath,
                       std::ios::binary | std::ios::app | std::ios::ate);
  if (!binary.is_open()) {
    if (outError)
      *outError = "Cannot open binary for embedding: " + binaryPath.string();
    return false;
  }

  binary.write(nconData.data(), static_cast<std::streamsize>(nconSize));

  // Write payload size as little-endian uint64
  uint64_t sizeLE = nconSize;
  binary.write(reinterpret_cast<const char *>(&sizeLE), 8);

  // Write magic
  binary.write(kEmbedMagic, 8);

  if (!binary.good()) {
    if (outError)
      *outError = "Write error during NCON embedding";
    return false;
  }

  return true;
}

bool hasEmbeddedNcon(const std::filesystem::path &binaryPath) {
  std::ifstream file(binaryPath, std::ios::binary | std::ios::ate);
  if (!file.is_open()) {
    return false;
  }

  auto fileSize = static_cast<uint64_t>(file.tellg());
  if (fileSize < kFooterSize) {
    return false;
  }

  file.seekg(-static_cast<std::streamoff>(kFooterSize), std::ios::end);

  // Skip size, read magic
  file.seekg(8, std::ios::cur);
  char magic[8];
  file.read(magic, 8);

  return std::memcmp(magic, kEmbedMagic, 8) == 0;
}

bool extractEmbeddedNcon(const std::filesystem::path &binaryPath,
                         std::vector<uint8_t> *outPayload,
                         std::string *outError) {
  if (outPayload == nullptr) {
    return false;
  }

  std::ifstream file(binaryPath, std::ios::binary | std::ios::ate);
  if (!file.is_open()) {
    if (outError)
      *outError = "Cannot open binary: " + binaryPath.string();
    return false;
  }

  auto fileSize = static_cast<uint64_t>(file.tellg());
  if (fileSize < kFooterSize) {
    if (outError)
      *outError = "Binary too small for embedded payload";
    return false;
  }

  // Read footer
  file.seekg(-static_cast<std::streamoff>(kFooterSize), std::ios::end);

  uint64_t payloadSize = 0;
  file.read(reinterpret_cast<char *>(&payloadSize), 8);

  char magic[8];
  file.read(magic, 8);

  if (std::memcmp(magic, kEmbedMagic, 8) != 0) {
    if (outError)
      *outError = "No embedded NCON payload found";
    return false;
  }

  if (payloadSize == 0 || payloadSize + kFooterSize > fileSize) {
    if (outError)
      *outError = "Invalid embedded payload size";
    return false;
  }

  // Seek to payload start and read
  file.seekg(-static_cast<std::streamoff>(kFooterSize + payloadSize),
             std::ios::end);
  outPayload->resize(payloadSize);
  file.read(reinterpret_cast<char *>(outPayload->data()),
            static_cast<std::streamsize>(payloadSize));

  if (!file.good()) {
    if (outError)
      *outError = "Failed to read embedded payload";
    return false;
  }

  return true;
}

// ─────────────────────────────────────────────
// Full product build pipeline
// ─────────────────────────────────────────────

ProductBuildResult buildProduct(ProductSettings &settings,
                                const ProductBuildOptions &options,
                                const std::filesystem::path &projectRoot,
                                const std::filesystem::path &toolRoot) {
  ProductBuildResult result;
  namespace fs = std::filesystem;

  const std::string neuronCli = resolveNeuronCliCommand(toolRoot);
  const std::string previousProductVersion = settings.productVersion;
  const int previousBuildVersion = settings.productBuildVersion;

  std::string versionBumpError;
  if (!incrementPatchVersion(settings.productVersion, &settings.productVersion,
                             &versionBumpError)) {
    result.errors.push_back("Invalid product_version '" +
                            previousProductVersion + "': " + versionBumpError);
    return result;
  }

  std::string originalSettingsContent;
  {
    std::ifstream file(projectRoot / ".productsettings");
    if (file.is_open()) {
      std::stringstream buffer;
      buffer << file.rdbuf();
      originalSettingsContent = buffer.str();
    }
  }

  const std::string versionTag = settings.productVersion;
  const fs::path outputDir = projectRoot / settings.outputDir / versionTag;
  fs::create_directories(outputDir);

  std::cout << "======================================" << std::endl;
  std::cout << "    Neuron++ Product Build" << std::endl;
  std::cout << "======================================" << std::endl;
  std::cout << "  Product: " << settings.productName << std::endl;
  std::cout << "  Version: " << versionTag << std::endl;
  std::cout << "  Platform: " << options.platformId << std::endl;
  std::cout << std::endl;

  std::cout << "[1/6] Building NCON container..." << std::endl;
  const fs::path nconPath = outputDir / (settings.outputName + ".ncon");
  {
    const std::string nconCmd =
        neuronCli + " ncon build -o " + quote(nconPath);
    if (runCmd(nconCmd, options.verbose) != 0) {
      result.errors.push_back("NCON build failed");
      return result;
    }
  }
  if (!fs::exists(nconPath)) {
    result.errors.push_back("NCON container not found at: " +
                            nconPath.string());
    return result;
  }

  std::cout << "[2/6] Compiling resources..." << std::endl;
  std::string resourceObj;
  const fs::path resourceObjPath = runResourceCompiler(
      settings, projectRoot, outputDir, options.platformId, options.verbose);
  if (!resourceObjPath.empty()) {
    resourceObj = resourceObjPath.string();
    std::cout << "  -> Compiled resources: " << resourceObj << std::endl;
  }

  std::cout << "[3/6] Building Nucleus runtime (" << options.platformId
            << ")..." << std::endl;
  const std::string exeExt =
      options.platform == MinimalTargetPlatform::WindowsX64 ? ".exe" : "";
  const fs::path productExe = outputDir / (settings.outputName + exeExt);
  {
    std::string miniCmd =
        neuronCli + " build-nucleus --platform " + options.platformId;
    if (!options.compilerPath.empty()) {
      miniCmd += " --compiler " + quote(options.compilerPath);
    }
    if (!resourceObj.empty()) {
      miniCmd += " --extra-obj " + quote(resourceObj);
    }
    miniCmd += " --output " + quote(productExe);
    if (options.verbose) {
      miniCmd += " --verbose";
    }
    if (runCmd(miniCmd, options.verbose) != 0) {
      result.errors.push_back("Nucleus build failed for " + options.platformId);
      return result;
    }
  }

  std::cout << "[4/6] Embedding NCON container..." << std::endl;
  {
    std::string embedError;
    if (!embedNconInBinary(productExe, nconPath, &embedError)) {
      result.errors.push_back("Embedding failed: " + embedError);
      return result;
    }
  }

  result.productExePath = productExe;
  std::cout << "  -> " << productExe.string() << std::endl;

  std::cout << "[5/6] Writing product manifest..." << std::endl;
  {
    const fs::path manifestPath = outputDir / "product-manifest.json";
    std::ofstream manifest(manifestPath);
    if (manifest.is_open()) {
      manifest << "{\n"
               << "  \"product_name\": \"" << settings.productName << "\",\n"
               << "  \"product_version\": \"" << settings.productVersion
               << "\",\n"
               << "  \"build_version\": " << settings.productBuildVersion
               << ",\n"
               << "  \"platform\": \"" << options.platformId << "\",\n"
               << "  \"executable\": \"" << productExe.filename().string()
               << "\",\n"
               << "  \"ncon_embedded\": true,\n"
               << "  \"update_enabled\": "
               << (settings.updateEnabled ? "true" : "false") << ",\n"
               << "  \"update_channel\": \"" << settings.updateChannel
               << "\"\n"
               << "}\n";
      result.manifestPath = manifestPath;
    }
  }

  if (settings.installerEnabled && !options.noInstaller) {
    if (options.platform == MinimalTargetPlatform::WindowsX64) {
      std::cout << "[6/6] Generating Windows Installer..." << std::endl;
      const InstallerBuildResult instResult = buildInstaller(
          settings, options, productExe, resourceObjPath, projectRoot,
          toolRoot);
      if (!instResult.errors.empty()) {
        for (const auto &err : instResult.errors) {
          result.errors.push_back("Installer error: " + err);
        }
        return result;
      }
      result.installerExePath = instResult.installerExePath;
      std::cout << "  -> " << instResult.installerExePath.string()
                << std::endl;
    } else {
      std::cout << "[6/6] Installer generation is skipped (only supported for Windows)."
                << std::endl;
    }
  } else {
    std::cout << "[6/6] Installer disabled via settings or --no-installer."
              << std::endl;
  }

  settings.productBuildVersion++;
  if (!originalSettingsContent.empty()) {
    if (!saveProductSettings(projectRoot / ".productsettings", settings,
                             originalSettingsContent)) {
      result.errors.push_back("Failed to save updated .productsettings");
      return result;
    }
    std::cout << "\n  [i] Auto-incremented product version: "
              << previousProductVersion << " -> " << settings.productVersion
              << std::endl;
    std::cout << "  [i] Auto-incremented build version: "
              << previousBuildVersion << " -> " << settings.productBuildVersion
              << std::endl;
  }

  std::cout << std::endl;
  std::cout << "Product build complete: " << productExe.string() << std::endl;

  result.success = true;
  return result;
}

} // namespace neuron
