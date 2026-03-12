#include "neuronc/mir/MIRBuilder.h"

namespace neuron::mir {

namespace {

std::string bindingKindName(BindingKind kind) {
  switch (kind) {
  case BindingKind::Alias: return "alias";
  case BindingKind::Copy: return "copy";
  case BindingKind::Value: return "value";
  case BindingKind::AddressOf: return "address_of";
  case BindingKind::ValueOf: return "value_of";
  case BindingKind::MoveFrom: return "move";
  }
  return "value";
}

} // namespace

void MIRBuilder::lowerAssignmentTarget(ASTNode *target, const Operand &value,
                                       const SourceLocation &location,
                                       const std::string &fallbackName,
                                       const std::string &note) {
  if (target == nullptr && !fallbackName.empty()) {
    if (isDeclared(fallbackName)) {
      emit({InstKind::Assign, location, resolveName(fallbackName), "", {},
            {value}, {}, note});
    } else {
      emit({InstKind::Bind, location, declare(fallbackName, location), "", {},
            {value}, {}, note});
    }
    return;
  }
  if (target != nullptr && target->type == ASTNodeType::Identifier) {
    lowerAssignmentTarget(nullptr, value, location,
                          static_cast<IdentifierNode *>(target)->name, note);
  } else if (target != nullptr && target->type == ASTNodeType::MemberAccessExpr) {
    auto *member = static_cast<MemberAccessNode *>(target);
    emit({InstKind::StoreMember, location, "", member->member, {},
          {lowerExpression(member->object.get()), value}, {}, note});
  } else if (target != nullptr && target->type == ASTNodeType::IndexExpr) {
    auto *index = static_cast<IndexExprNode *>(target);
    std::vector<Operand> operands{lowerExpression(index->object.get())};
    for (auto &item : index->indices) operands.push_back(lowerExpression(item.get()));
    operands.push_back(value);
    emit({InstKind::StoreIndex, location, "", "", {}, std::move(operands), {},
          note});
  } else {
    unsupportedStmt(target, "unsupported assignment target");
  }
}

Operand MIRBuilder::lowerBindingValue(BindingDeclNode *node) {
  if (node == nullptr || node->value == nullptr) {
    return constantTemp(node != nullptr ? node->location : SourceLocation{},
                        "<uninitialized>");
  }
  if (node->kind == BindingKind::MoveFrom) {
    if (node->value->type != ASTNodeType::Identifier) {
      return unsupportedExpr(node->value.get(), "move requires a named variable");
    }
    Operand result = nextTemp();
    emit({InstKind::Move, node->location, result.text, "", {},
          {Operand::variable(
              resolveName(static_cast<IdentifierNode *>(node->value.get())->name))}});
    return result;
  }
  if (node->kind == BindingKind::AddressOf) {
    if (node->value->type != ASTNodeType::Identifier) {
      return unsupportedExpr(node->value.get(),
                             "address_of requires a named variable");
    }
    Operand result = nextTemp();
    emit({InstKind::Borrow, node->location, result.text, "", {},
          {Operand::variable(
              resolveName(static_cast<IdentifierNode *>(node->value.get())->name))}});
    return result;
  }
  if (node->kind == BindingKind::ValueOf) {
    Operand result = nextTemp();
    emit({InstKind::Deref, node->location, result.text, "", {},
          {lowerExpression(node->value.get())}});
    return result;
  }
  return lowerExpression(node->value.get());
}

void MIRBuilder::lowerBinding(BindingDeclNode *node) {
  const std::string note = "binding:" + bindingKindName(node->kind);
  if (node->name == "__deref__") {
    unsupportedStmt(node, "value_of assignment is not supported in MIR yet");
    return;
  }
  Operand value = lowerBindingValue(node);
  if (node->name == "__assign__") {
    lowerAssignmentTarget(node->target.get(), value, node->location, "", note);
    return;
  }
  if (node->target == nullptr) {
    emit({InstKind::Bind, node->location, declare(node->name, node->location),
          "", {}, {value}, {}, note});
    return;
  }
  if (node->target->type == ASTNodeType::Identifier) {
    const auto *identifier = static_cast<IdentifierNode *>(node->target.get());
    emit({InstKind::Bind, node->location,
          declare(identifier->name, identifier->location), "", {}, {value}, {},
          note});
    return;
  }
  lowerAssignmentTarget(node->target.get(), value, node->location, node->name,
                        note);
}

void MIRBuilder::lowerUpdate(ASTNode *node, const std::string &name, std::string op) {
  const std::string resolved = resolveName(name);
  Operand next = nextTemp();
  emit({InstKind::Binary, node->location, next.text, std::move(op), {},
        {copyTemp(node->location, resolved), constantTemp(node->location, "1")}});
  emit({InstKind::Assign, node->location, resolved, "", {}, {next}});
}

void MIRBuilder::lowerCast(CastStmtNode *node) {
  Operand current = lowerExpression(node->target.get());
  for (const auto &step : node->steps) {
    Operand next = nextTemp();
    emit({InstKind::Cast, step.location, next.text, typeText(step.typeSpec.get()),
          {}, {current}, {}, step.allowNullOnFailure ? "nullable" : ""});
    current = next;
  }
  lowerAssignmentTarget(node->target.get(), current, node->location, "",
                        "binding:assign:value");
}

void MIRBuilder::lowerStatement(ASTNode *node) {
  if (node == nullptr || block().terminated) return;
  switch (node->type) {
  case ASTNodeType::Block: lowerBlock(node, true); return;
  case ASTNodeType::BindingDecl: lowerBinding(static_cast<BindingDeclNode *>(node)); return;
  case ASTNodeType::IfStmt: lowerIf(static_cast<IfStmtNode *>(node)); return;
  case ASTNodeType::WhileStmt: lowerWhile(static_cast<WhileStmtNode *>(node)); return;
  case ASTNodeType::ForStmt: lowerFor(static_cast<ForStmtNode *>(node)); return;
  case ASTNodeType::ForInStmt: lowerForIn(static_cast<ForInStmtNode *>(node)); return;
  case ASTNodeType::MatchStmt: lowerMatchStatement(static_cast<MatchStmtNode *>(node)); return;
  case ASTNodeType::ReturnStmt: emitReturn(node->location, static_cast<ReturnStmtNode *>(node)->value ? lowerExpression(static_cast<ReturnStmtNode *>(node)->value.get()) : Operand{}); return;
  case ASTNodeType::BreakStmt: if (!m_loopStack.empty()) emitJump(node->location, m_loopStack.back().breakBlock); else unsupportedStmt(node, "break outside loop"); return;
  case ASTNodeType::ContinueStmt: if (!m_loopStack.empty()) emitJump(node->location, m_loopStack.back().continueBlock); else unsupportedStmt(node, "continue outside loop"); return;
  case ASTNodeType::IncrementStmt: lowerUpdate(node, static_cast<IncrementStmtNode *>(node)->variable, "+"); return;
  case ASTNodeType::DecrementStmt: lowerUpdate(node, static_cast<DecrementStmtNode *>(node)->variable, "-"); return;
  case ASTNodeType::CastStmt: lowerCast(static_cast<CastStmtNode *>(node)); return;
  case ASTNodeType::UnsafeBlock: lowerBlock(static_cast<UnsafeBlockNode *>(node)->body.get(), true); return;
  case ASTNodeType::GpuBlock: lowerBlock(static_cast<GpuBlockNode *>(node)->body.get(), true); return;
  case ASTNodeType::IntLiteral:
  case ASTNodeType::FloatLiteral:
  case ASTNodeType::StringLiteral:
  case ASTNodeType::BoolLiteral:
  case ASTNodeType::NullLiteral:
  case ASTNodeType::Identifier:
  case ASTNodeType::BinaryExpr:
  case ASTNodeType::UnaryExpr:
  case ASTNodeType::CallExpr:
  case ASTNodeType::InputExpr:
  case ASTNodeType::MemberAccessExpr:
  case ASTNodeType::IndexExpr:
  case ASTNodeType::SliceExpr:
  case ASTNodeType::TypeofExpr:
  case ASTNodeType::MatchExpr:
    lowerExpression(node);
    return;
  default:
    unsupportedStmt(node, "unsupported statement: " + describeNode(node));
    return;
  }
}

} // namespace neuron::mir
