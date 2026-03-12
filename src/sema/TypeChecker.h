#pragma once

#include "neuronc/parser/AST.h"
#include "neuronc/sema/TypeSystem.h"

namespace neuron::sema_detail {

class AnalysisContext;

class TypeChecker {
public:
  bool canAssignType(const NTypePtr &target, const NTypePtr &source) const;
  NTypePtr applyCastPipeline(AnalysisContext &context, NTypePtr sourceType,
                             const CastStmtNode *node,
                             ASTNode *sourceExpr) const;
};

} // namespace neuron::sema_detail
