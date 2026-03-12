#include "neuronc/nir/NIRBuilder.h"

namespace neuron::nir {

Value *NIRBuilder::buildExpression(ASTNode *node) {
  if (!node)
    return nullptr;
  switch (node->type) {
  case ASTNodeType::IntLiteral:
    return new ConstantInt(static_cast<IntLiteralNode *>(node)->value);
  case ASTNodeType::FloatLiteral:
    return new ConstantFloat(static_cast<FloatLiteralNode *>(node)->value);
  case ASTNodeType::StringLiteral:
    return new ConstantString(static_cast<StringLiteralNode *>(node)->value);
  case ASTNodeType::BoolLiteral:
    return new ConstantInt(static_cast<BoolLiteralNode *>(node)->value ? 1 : 0);
  case ASTNodeType::NullLiteral:
    return new ConstantNull();
  case ASTNodeType::Identifier:
    return buildIdentifier(static_cast<IdentifierNode *>(node));
  case ASTNodeType::BinaryExpr:
    return buildBinaryExpr(static_cast<BinaryExprNode *>(node));
  case ASTNodeType::UnaryExpr:
    return buildUnaryExpr(static_cast<UnaryExprNode *>(node));
  case ASTNodeType::CallExpr:
    return buildCallExpr(static_cast<CallExprNode *>(node));
  case ASTNodeType::InputExpr:
    return buildInputExpr(static_cast<InputExprNode *>(node));
  case ASTNodeType::MemberAccessExpr:
    return buildMemberAccess(static_cast<MemberAccessNode *>(node));
  case ASTNodeType::SliceExpr:
    return buildSliceExpr(static_cast<SliceExprNode *>(node));
  case ASTNodeType::MatchExpr:
    return buildMatchExpr(static_cast<MatchExprNode *>(node));
  case ASTNodeType::TypeSpec:
    return new ConstantString(static_cast<TypeSpecNode *>(node)->typeName);
  case ASTNodeType::TypeofExpr: {
    auto *typeofNode = static_cast<TypeofExprNode *>(node);
    if (typeofNode->expression == nullptr) {
      return new ConstantString("unknown");
    }
    switch (typeofNode->expression->type) {
    case ASTNodeType::IntLiteral:
      return new ConstantString("int");
    case ASTNodeType::FloatLiteral:
      return new ConstantString("float");
    case ASTNodeType::StringLiteral:
      return new ConstantString("string");
    case ASTNodeType::BoolLiteral:
      return new ConstantString("bool");
    default:
      return new ConstantString("unknown");
    }
  }
  default:
    return nullptr;
  }
}

Value *NIRBuilder::buildIdentifier(IdentifierNode *node) {
  Value *ptr = lookupSymbol(node->name);
  if (!ptr) {
    if (node->name == "Print") {
      return new ConstantString("Print");
    }
    for (const auto &func : m_module->getFunctions()) {
      if (func->getName() == node->name) {
        return new ConstantString(node->name);
      }
    }
    reportError(node->location, "Undefined symbol: " + node->name);
    return nullptr;
  }

  if ((ptr->getType() && ptr->getType()->kind == TypeKind::Pointer) ||
      ptr->getValueKind() == ValueKind::GlobalVariable) {
    NTypePtr loadResultType = NType::makeUnknown();
    if (ptr->getType() && ptr->getType()->kind == TypeKind::Pointer) {
      loadResultType = ptr->getType()->pointeeType;
    } else if (ptr->getValueKind() == ValueKind::GlobalVariable) {
      loadResultType = ptr->getType();
    }
    Instruction *load =
        createInst(InstKind::Load, loadResultType, node->name + "_val");
    load->addOperand(ptr);
    return load;
  }

  return ptr;
}

} // namespace neuron::nir
