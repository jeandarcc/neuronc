#pragma once

#include "neuronc/cli/ProductSettings.h"
#include <filesystem>
#include <string>

namespace neuron {

/// Compiles platform-specific resources (icons, version info) and returns
/// the path to the resulting object file or resource file to link.
/// Returns empty path on failure.
std::filesystem::path
runResourceCompiler(const ProductSettings &settings,
                    const std::filesystem::path &projectRoot,
                    const std::filesystem::path &outputDir,
                    const std::string &platformId, bool verbose);

} // namespace neuron
