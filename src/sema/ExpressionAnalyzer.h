#pragma once

#include "neuronc/sema/TypeSystem.h"

namespace neuron {
class ASTNode;
class BinaryExprNode;
class CallExprNode;
class IdentifierNode;
class IndexExprNode;
class MatchExprNode;
class MemberAccessNode;
class MethodDeclNode;
class SliceExprNode;
class TypeofExprNode;
class UnaryExprNode;
} // namespace neuron

namespace neuron::sema_detail {

class SemanticDriver;

class ExpressionAnalyzer {
public:
  explicit ExpressionAnalyzer(SemanticDriver &driver);

  NTypePtr infer(ASTNode *expr);
  NTypePtr inferMethodDecl(MethodDeclNode *node);

private:
  NTypePtr inferIdentifier(IdentifierNode *node);
  NTypePtr inferCallExpr(CallExprNode *node);
  NTypePtr inferMatchExpr(MatchExprNode *node);
  NTypePtr inferMemberAccess(MemberAccessNode *node);
  NTypePtr inferIndexExpr(IndexExprNode *node);
  NTypePtr inferSliceExpr(SliceExprNode *node);
  NTypePtr inferTypeofExpr(TypeofExprNode *node);
  NTypePtr inferBinaryExpr(BinaryExprNode *node);
  NTypePtr inferUnaryExpr(UnaryExprNode *node);

  SemanticDriver &m_driver;
};

} // namespace neuron::sema_detail
