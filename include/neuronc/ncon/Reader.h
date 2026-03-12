#pragma once

#include "neuronc/ncon/Format.h"

#include <filesystem>
#include <string>

namespace neuron::ncon {

bool loadContainer(const std::filesystem::path &inputPath,
                   ContainerData *outContainer, std::string *outError);

} // namespace neuron::ncon
