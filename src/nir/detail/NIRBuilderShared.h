#pragma once

#include "neuronc/nir/NIRBuilder.h"

#include <sstream>
#include <vector>

namespace neuron::nir::detail {

inline bool blockHasTerminator(const Block *block) {
  if (block == nullptr) {
    return false;
  }
  const auto &insts = block->getInstructions();
  if (insts.empty()) {
    return false;
  }
  InstKind kind = insts.back()->getKind();
  return kind == InstKind::Ret || kind == InstKind::Br ||
         kind == InstKind::CondBr;
}

inline bool isTensorInstKind(InstKind kind) {
  switch (kind) {
  case InstKind::TensorAdd:
  case InstKind::TensorSub:
  case InstKind::TensorMul:
  case InstKind::TensorDiv:
  case InstKind::TensorMatMul:
  case InstKind::TensorMatMulAdd:
  case InstKind::TensorLinearFused:
  case InstKind::TensorSlice:
  case InstKind::TensorFMA:
    return true;
  default:
    return false;
  }
}

inline void appendBuilderError(std::vector<std::string> &errors, bool &hadError,
                               const SourceLocation &location,
                               const std::string &message) {
  hadError = true;
  std::ostringstream oss;
  if (!location.file.empty()) {
    oss << location.file << ":" << location.line << ":" << location.column
        << ": error: ";
  }
  oss << message;
  errors.push_back(oss.str());
  // Note: This helper also writes to stderr. This is to keep errors visible
  // during NIR build even outside of tests. The "Undefined symbol" output
  // seen in tests like `NirBuilderReportsUndefinedSymbolErrors` is
  // expected and intentional behavior; the test verifies that this error
  // is collected, and the stderr line alone does not indicate a test failure.
}

} // namespace neuron::nir::detail
