#pragma once

#include "MIRNode.h"

#include <iosfwd>
#include <string>

namespace neuron::mir {

void print(std::ostream &out, const Module &module);
std::string printToString(const Module &module);

} // namespace neuron::mir
