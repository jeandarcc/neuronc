#pragma once

#include "neuronc/sema/TypeSystem.h"

namespace neuron {
class CallExprNode;
class InputExprNode;
} // namespace neuron

namespace neuron::sema_detail {

class SemanticDriver;

class InputAnalyzer {
public:
  explicit InputAnalyzer(SemanticDriver &driver);

  NTypePtr infer(InputExprNode *node);
  NTypePtr tryInferCallChain(CallExprNode *node);

private:
  SemanticDriver &m_driver;
};

} // namespace neuron::sema_detail
