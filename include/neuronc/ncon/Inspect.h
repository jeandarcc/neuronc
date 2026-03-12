#pragma once

#include "neuronc/ncon/Format.h"

#include <string>

namespace neuron::ncon {

std::string inspectContainerHuman(const ContainerData &container);
std::string inspectContainerJson(const ContainerData &container);

} // namespace neuron::ncon
