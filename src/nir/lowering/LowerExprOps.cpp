#include "neuronc/nir/NIRBuilder.h"

#include "../detail/NIRBuilderShared.h"

namespace neuron::nir {

Value *NIRBuilder::buildBinaryExpr(BinaryExprNode *node) {
  if (node == nullptr) {
    return nullptr;
  }
  Value *lhs = buildExpression(node->left.get());
  Value *rhs = buildExpression(node->right.get());
  if (lhs == nullptr || rhs == nullptr) {
    return nullptr;
  }

  InstKind kind = InstKind::Add;
  NTypePtr resultType = lhs->getType() ? lhs->getType() : NType::makeUnknown();
  const bool lhsTensor =
      resultType != nullptr && resultType->kind == TypeKind::Tensor;
  switch (node->op) {
  case TokenType::Plus:
    if (lhsTensor) {
      kind = InstKind::TensorAdd;
    } else {
      kind = (resultType && resultType->kind == TypeKind::Float)
                 ? InstKind::FAdd
                 : InstKind::Add;
    }
    break;
  case TokenType::Minus:
    if (lhsTensor) {
      kind = InstKind::TensorSub;
    } else {
      kind = (resultType && resultType->kind == TypeKind::Float)
                 ? InstKind::FSub
                 : InstKind::Sub;
    }
    break;
  case TokenType::Star:
    if (lhsTensor) {
      kind = InstKind::TensorMul;
    } else {
      kind = (resultType && resultType->kind == TypeKind::Float)
                 ? InstKind::FMul
                 : InstKind::Mul;
    }
    break;
  case TokenType::Slash:
    if (lhsTensor) {
      kind = InstKind::TensorDiv;
    } else {
      kind = (resultType && resultType->kind == TypeKind::Float)
                 ? InstKind::FDiv
                 : InstKind::Div;
    }
    break;
  case TokenType::Caret:
    kind = InstKind::Pow;
    resultType = lhs->getType() ? lhs->getType() : NType::makeFloat();
    break;
  case TokenType::CaretCaret:
    kind = InstKind::NthRoot;
    resultType = lhs->getType() ? lhs->getType() : NType::makeFloat();
    if (rhs->getValueKind() == ValueKind::ConstantInt &&
        static_cast<ConstantInt *>(rhs)->getValue() == 2) {
      kind = InstKind::Sqrt;
    }
    break;
  case TokenType::EqualEqual:
    kind = InstKind::Eq;
    resultType = NType::makeBool();
    break;
  case TokenType::NotEqual:
    kind = InstKind::Neq;
    resultType = NType::makeBool();
    break;
  case TokenType::Greater:
    kind = InstKind::Gt;
    resultType = NType::makeBool();
    break;
  case TokenType::GreaterEqual:
    kind = InstKind::Gte;
    resultType = NType::makeBool();
    break;
  case TokenType::Less:
    kind = InstKind::Lt;
    resultType = NType::makeBool();
    break;
  case TokenType::LessEqual:
    kind = InstKind::Lte;
    resultType = NType::makeBool();
    break;
  case TokenType::At:
    kind = InstKind::TensorMatMul;
    resultType = lhs->getType();
    break;
  default:
    return nullptr;
  }

  Instruction *inst = createInst(kind, resultType, nextValName());
  inst->addOperand(lhs);
  if (kind != InstKind::Sqrt) {
    inst->addOperand(rhs);
  }
  return inst;
}

Value *NIRBuilder::buildUnaryExpr(UnaryExprNode *node) {
  if (node == nullptr || node->operand == nullptr) {
    return nullptr;
  }

  if (node->op == TokenType::ValueOf) {
    Value *ptr = buildExpression(node->operand.get());
    if (ptr == nullptr) {
      return nullptr;
    }
    NTypePtr pointee = NType::makeUnknown();
    if (ptr->getType() && ptr->getType()->kind == TypeKind::Pointer) {
      pointee = ptr->getType()->pointeeType;
    }
    Instruction *load = createInst(InstKind::Load, pointee, nextValName());
    load->addOperand(ptr);
    return load;
  }

  Value *operand = buildExpression(node->operand.get());
  if (operand == nullptr) {
    return nullptr;
  }

  if (node->op == TokenType::Minus) {
    Instruction *neg = createInst(InstKind::Sub, operand->getType(), nextValName());
    neg->addOperand(new ConstantInt(0));
    neg->addOperand(operand);
    return neg;
  }

  if (node->op == TokenType::Not) {
    Instruction *cmp = createInst(InstKind::Eq, NType::makeBool(), nextValName());
    cmp->addOperand(operand);
    cmp->addOperand(new ConstantInt(0));
    return cmp;
  }

  if (node->op == TokenType::Await) {
    return operand;
  }

  return operand;
}

Value *NIRBuilder::buildAddressOf(ASTNode *node) { return buildLValue(node); }

Value *NIRBuilder::buildMemberAccess(MemberAccessNode *node) {
  if (node == nullptr) {
    return nullptr;
  }

  if (node->object && node->object->type == ASTNodeType::Identifier) {
    auto *ident = static_cast<IdentifierNode *>(node->object.get());
    auto enumIt = m_enumMembers.find(ident->name);
    if (enumIt != m_enumMembers.end()) {
      auto memberIt = enumIt->second.find(node->member);
      if (memberIt != enumIt->second.end()) {
        return new ConstantInt(memberIt->second);
      }
    }
  }

  Value *basePtr = buildLValue(node->object.get());
  if (basePtr == nullptr) {
    return nullptr;
  }

  int64_t fieldIndex = 0;
  if (basePtr->getType() && basePtr->getType()->kind == TypeKind::Pointer &&
      basePtr->getType()->pointeeType &&
      basePtr->getType()->pointeeType->kind == TypeKind::Class) {
    const std::string &className = basePtr->getType()->pointeeType->className;
    for (const auto &cls : m_module->getClasses()) {
      if (cls->getName() != className) {
        continue;
      }
      const auto &fields = cls->getFields();
      for (size_t i = 0; i < fields.size(); ++i) {
        if (fields[i].name == node->member) {
          fieldIndex = static_cast<int64_t>(i);
          break;
        }
      }
    }
  }

  Instruction *fieldPtr =
      createInst(InstKind::FieldAccess, NType::makePointer(NType::makeUnknown()),
                 nextValName());
  fieldPtr->addOperand(basePtr);
  fieldPtr->addOperand(new ConstantInt(fieldIndex));

  Instruction *load = createInst(InstKind::Load, NType::makeUnknown(), nextValName());
  load->addOperand(fieldPtr);
  return load;
}

Value *NIRBuilder::buildSliceExpr(SliceExprNode *node) {
  if (node == nullptr) {
    return nullptr;
  }
  Value *base = buildExpression(node->object.get());
  Value *start = buildExpression(node->start.get());
  Value *end = buildExpression(node->end.get());
  if (base == nullptr || start == nullptr || end == nullptr) {
    return nullptr;
  }

  Instruction *slice = createInst(InstKind::TensorSlice, base->getType(), nextValName());
  slice->addOperand(base);
  slice->addOperand(start);
  slice->addOperand(end);
  return slice;
}

Value *NIRBuilder::buildLValue(ASTNode *node) {
  if (node == nullptr) {
    return nullptr;
  }

  if (node->type == ASTNodeType::Identifier) {
    return lookupSymbol(static_cast<IdentifierNode *>(node)->name);
  }

  if (node->type == ASTNodeType::MemberAccessExpr) {
    auto *member = static_cast<MemberAccessNode *>(node);
    Value *basePtr = buildLValue(member->object.get());
    if (basePtr == nullptr) {
      return nullptr;
    }

    int64_t fieldIndex = 0;
    if (basePtr->getType() && basePtr->getType()->kind == TypeKind::Pointer &&
        basePtr->getType()->pointeeType &&
        basePtr->getType()->pointeeType->kind == TypeKind::Class) {
      const std::string &className = basePtr->getType()->pointeeType->className;
      for (const auto &cls : m_module->getClasses()) {
        if (cls->getName() != className) {
          continue;
        }
        const auto &fields = cls->getFields();
        for (size_t i = 0; i < fields.size(); ++i) {
          if (fields[i].name == member->member) {
            fieldIndex = static_cast<int64_t>(i);
            break;
          }
        }
      }
    }

    Instruction *fieldPtr = createInst(
        InstKind::FieldAccess, NType::makePointer(NType::makeUnknown()),
        nextValName());
    fieldPtr->addOperand(basePtr);
    fieldPtr->addOperand(new ConstantInt(fieldIndex));
    return fieldPtr;
  }

  return buildExpression(node);
}

} // namespace neuron::nir

