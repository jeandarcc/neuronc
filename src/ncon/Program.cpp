#include "neuronc/ncon/Program.h"

namespace neuron::ncon {

std::string opcodeName(Opcode opcode) {
  switch (opcode) {
#define X(name, value)                                                           \
  case Opcode::name:                                                             \
    return #name;
#include "neuronc/ncon/Opcodes.def"
#undef X
  }
  return "Unknown";
}

} // namespace neuron::ncon
