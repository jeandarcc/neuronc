#pragma once

#include "neuronc/cli/ProductBuilder.h"
#include "neuronc/cli/ProductSettings.h"
#include <filesystem>
#include <string>
#include <vector>


namespace neuron {

struct InstallerBuildResult {
  std::filesystem::path installerExePath;
  std::vector<std::string> errors;
};

InstallerBuildResult
buildInstaller(const ProductSettings &settings,
               const ProductBuildOptions &options,
               const std::filesystem::path &productExePath,
               const std::filesystem::path &resourceObjPath,
               const std::filesystem::path &projectRoot,
               const std::filesystem::path &toolRoot);

} // namespace neuron
