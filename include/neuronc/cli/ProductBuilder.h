#pragma once

#include "neuronc/cli/MinimalBuilder.h"
#include "neuronc/cli/ProductSettings.h"

#include <filesystem>
#include <string>
#include <vector>

namespace neuron {

/// Options for the build-product command.
struct ProductBuildOptions {
  MinimalTargetPlatform platform = MinimalTargetPlatform::Unknown;
  std::string platformId;
  std::string compilerPath;
  bool noInstaller = false;
  bool noUpdater = false;
  bool verbose = false;
};

/// Result of a product build step.
struct ProductBuildResult {
  bool success = false;
  std::filesystem::path productExePath;
  std::filesystem::path installerExePath;
  std::filesystem::path manifestPath;
  std::vector<std::string> errors;
};

/// Parse build-product CLI arguments.
bool parseProductBuildArgs(const std::vector<std::string> &args,
                           const std::string &hostPlatformId,
                           ProductBuildOptions *outOptions,
                           std::string *outError);

/// Run the full product build pipeline.
ProductBuildResult buildProduct(ProductSettings &settings,
                                const ProductBuildOptions &options,
                                const std::filesystem::path &projectRoot,
                                const std::filesystem::path &toolRoot);

/// Embed a .ncon container into a Nucleus binary (self-extracting pattern).
bool embedNconInBinary(const std::filesystem::path &binaryPath,
                       const std::filesystem::path &nconPath,
                       std::string *outError);

/// Check if a binary has an embedded NCON payload.
bool hasEmbeddedNcon(const std::filesystem::path &binaryPath);

/// Extract the embedded NCON payload from a binary into memory.
bool extractEmbeddedNcon(const std::filesystem::path &binaryPath,
                         std::vector<uint8_t> *outPayload,
                         std::string *outError);

} // namespace neuron
