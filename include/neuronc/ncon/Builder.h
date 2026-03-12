#pragma once

#include "neuronc/ncon/Format.h"

#include <filesystem>
#include <string>

namespace neuron::ncon {

struct BuildRequest {
  std::filesystem::path inputPath;
  std::filesystem::path outputPath;
};

bool buildContainerFromInput(const BuildRequest &request,
                             std::filesystem::path *outPath,
                             std::string *outError);

} // namespace neuron::ncon
