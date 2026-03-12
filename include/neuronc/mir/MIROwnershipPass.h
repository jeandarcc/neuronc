#pragma once

#include "MIRNode.h"
#include "neuronc/sema/DiagnosticEmitter.h"

#include <string>
#include <vector>

namespace neuron::mir {

struct OwnershipHint {
  SourceLocation location;
  int length = 1;
  std::string label;
  std::string tooltip;
};

class MIROwnershipPass {
public:
  void reset();
  void setSourceText(std::string sourceText);
  void setAgentHints(bool enabled);
  void run(const Module &module);

  const std::vector<SemanticError> &errors() const;
  const std::vector<OwnershipHint> &hints() const;
  bool hasErrors() const;

private:
  sema_detail::DiagnosticEmitter m_diagnostics;
  std::vector<OwnershipHint> m_hints;
};

} // namespace neuron::mir
