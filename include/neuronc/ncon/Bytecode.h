#pragma once

#include "neuronc/ncon/Program.h"
#include "neuronc/nir/NIR.h"

#include <string>
#include <unordered_set>

namespace neuron::ncon {

struct LowerToProgramOptions {
  std::unordered_set<std::string> nativeCallTargets;
};

bool lowerToProgram(const neuron::nir::Module &module, Program *outProgram,
                    std::string *outError,
                    const LowerToProgramOptions &options = {});
bool opcodeFromInstruction(neuron::nir::InstKind kind, Opcode *outOpcode);

} // namespace neuron::ncon
