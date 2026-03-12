#pragma once

#include "neuronc/fusion/FusionBuiltins.h"
#include "neuronc/parser/AST.h"

namespace neuron {

inline const CallExprNode *fusionBaseCall(const CallExprNode *node) {
  if (node == nullptr || !node->isFusionChain || node->fusionCallNames.empty()) {
    return nullptr;
  }

  const CallExprNode *current = node;
  for (std::size_t i = 1; i < node->fusionCallNames.size(); ++i) {
    if (current->arguments.size() != 1 || current->arguments[0] == nullptr ||
        current->arguments[0]->type != ASTNodeType::CallExpr) {
      return nullptr;
    }
    current = static_cast<const CallExprNode *>(current->arguments[0].get());
  }
  return current;
}

inline CallExprNode *fusionBaseCall(CallExprNode *node) {
  return const_cast<CallExprNode *>(
      fusionBaseCall(static_cast<const CallExprNode *>(node)));
}

inline const FusionPatternSpec *matchFusionChain(const CallExprNode *node) {
  if (node == nullptr || !node->isFusionChain) {
    return nullptr;
  }
  return matchFusionPatternSpec(node->fusionCallNames);
}

} // namespace neuron
