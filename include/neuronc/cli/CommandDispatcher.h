#pragma once

#include "neuronc/cli/App.h"

namespace neuron::cli {

int dispatchCommand(AppContext &context, const AppServices &services, int argc,
                    char *argv[]);

} // namespace neuron::cli

