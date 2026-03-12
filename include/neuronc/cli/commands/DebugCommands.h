#pragma once

#include "neuronc/cli/DebugSupport.h"

namespace neuron::cli {

int runLexCommand(const std::string &filepath,
                  const DebugCommandDependencies &deps);
int runParseCommand(const std::string &filepath,
                    const DebugCommandDependencies &deps);
int runNirCommand(const std::string &filepath,
                  const DebugCommandDependencies &deps);

} // namespace neuron::cli

