#include "neuronc/mir/MIRBuilder.h"

#include <algorithm>

namespace neuron::mir {

namespace {

std::string quote(std::string text) { return "\"" + text + "\""; }

std::string tokenText(TokenType type) {
  switch (type) {
  case TokenType::Plus: return "+";
  case TokenType::Minus: return "-";
  case TokenType::Star: return "*";
  case TokenType::Slash: return "/";
  case TokenType::At: return "@";
  case TokenType::EqualEqual: return "==";
  case TokenType::NotEqual: return "!=";
  case TokenType::Greater: return ">";
  case TokenType::Less: return "<";
  case TokenType::GreaterEqual: return ">=";
  case TokenType::LessEqual: return "<=";
  case TokenType::And: return "&&";
  case TokenType::Or: return "||";
  case TokenType::Not: return "!";
  default: return tokenTypeName(type);
  }
}

} // namespace

void MIRBuilder::lowerIf(IfStmtNode *node) {
  Operand condition = lowerCondition(node->condition.get());
  const std::size_t thenBlock = createBlock("if_then");
  const std::size_t elseBlock =
      node->elseBlock ? createBlock("if_else") : createBlock("if_exit");
  const std::size_t exitBlock =
      node->elseBlock ? createBlock("if_exit") : elseBlock;
  emitBranch(node->location, condition, thenBlock, elseBlock);
  switchTo(thenBlock);
  lowerBlock(node->thenBlock.get(), true);
  if (!block().terminated) emitJump(node->location, exitBlock);
  if (node->elseBlock) {
    switchTo(elseBlock);
    lowerBlock(node->elseBlock.get(), true);
    if (!block().terminated) emitJump(node->location, exitBlock);
  }
  switchTo(exitBlock);
}

void MIRBuilder::lowerWhile(WhileStmtNode *node) {
  const std::size_t condBlock = createBlock("while_cond");
  const std::size_t bodyBlock = createBlock("while_body");
  const std::size_t exitBlock = createBlock("while_exit");
  emitJump(node->location, condBlock);
  switchTo(condBlock);
  emitBranch(node->location, lowerCondition(node->condition.get()), bodyBlock,
             exitBlock);
  m_loopStack.push_back({condBlock, exitBlock});
  switchTo(bodyBlock);
  lowerBlock(node->body.get(), true);
  if (!block().terminated) emitJump(node->location, condBlock);
  m_loopStack.pop_back();
  switchTo(exitBlock);
}

void MIRBuilder::lowerFor(ForStmtNode *node) {
  if (node->init) lowerStatement(node->init.get());
  const std::size_t condBlock = createBlock("for_cond");
  const std::size_t bodyBlock = createBlock("for_body");
  const std::size_t incBlock = createBlock("for_inc");
  const std::size_t exitBlock = createBlock("for_exit");
  emitJump(node->location, condBlock);
  switchTo(condBlock);
  if (node->condition) {
    emitBranch(node->location, lowerCondition(node->condition.get()), bodyBlock,
               exitBlock);
  } else {
    emitJump(node->location, bodyBlock);
  }
  m_loopStack.push_back({incBlock, exitBlock});
  switchTo(bodyBlock);
  lowerBlock(node->body.get(), true);
  if (!block().terminated) emitJump(node->location, incBlock);
  switchTo(incBlock);
  if (node->increment) lowerStatement(node->increment.get());
  if (!block().terminated) emitJump(node->location, condBlock);
  m_loopStack.pop_back();
  switchTo(exitBlock);
}

void MIRBuilder::lowerForIn(ForInStmtNode *node) {
  Operand iterator = nextTemp();
  emit({InstKind::Call, node->location, iterator.text, "", Operand::label("iter_init"),
        {lowerExpression(node->iterable.get())}});
  const std::size_t condBlock = createBlock("for_in_cond");
  const std::size_t bodyBlock = createBlock("for_in_body");
  const std::size_t exitBlock = createBlock("for_in_exit");
  emitJump(node->location, condBlock);
  switchTo(condBlock);
  Operand hasNext = nextTemp();
  emit({InstKind::Call, node->location, hasNext.text, "",
        Operand::label("iter_has_next"), {iterator}});
  emitBranch(node->location, hasNext, bodyBlock, exitBlock);
  m_loopStack.push_back({condBlock, exitBlock});
  switchTo(bodyBlock);
  pushScope();
  const std::string loopVariable =
      declare(node->variable, node->variableLocation);
  Operand nextValue = nextTemp();
  emit({InstKind::Call, node->location, nextValue.text, "",
        Operand::label("iter_next"), {iterator}});
  emit({InstKind::Bind, node->variableLocation, loopVariable, "", {}, {nextValue},
        {}, "binding:value"});
  lowerBlock(node->body.get(), false);
  popScope();
  if (!block().terminated) emitJump(node->location, condBlock);
  m_loopStack.pop_back();
  switchTo(exitBlock);
}

Operand MIRBuilder::lowerMatchCondition(const std::vector<Operand> &selectors,
                                        MatchArmNode *arm) {
  if (arm == nullptr || arm->isDefault || arm->patternExprs.empty()) {
    return constantTemp({}, "true");
  }
  Operand combined;
  for (std::size_t i = 0;
       i < std::min(selectors.size(), arm->patternExprs.size()); ++i) {
    Operand eq = nextTemp();
    emit({InstKind::Binary, arm->location, eq.text, "==", {},
          {selectors[i], lowerExpression(arm->patternExprs[i].get())}});
    if (combined.kind == OperandKind::None) {
      combined = eq;
    } else {
      Operand next = nextTemp();
      emit({InstKind::Binary, arm->location, next.text, "&&", {},
            {combined, eq}});
      combined = next;
    }
  }
  return combined.kind == OperandKind::None ? constantTemp(arm->location, "true")
                                            : combined;
}

void MIRBuilder::lowerMatchStatement(MatchStmtNode *node) {
  std::vector<Operand> selectors;
  for (auto &expr : node->expressions) selectors.push_back(lowerExpression(expr.get()));
  const std::size_t exitBlock = createBlock("match_exit");
  for (std::size_t i = 0; i < node->arms.size(); ++i) {
    auto *arm = static_cast<MatchArmNode *>(node->arms[i].get());
    const std::size_t armBlock = createBlock("match_arm");
    const bool last = i + 1 == node->arms.size();
    const std::size_t nextBlock =
        arm->isDefault || last ? exitBlock : createBlock("match_test");
    arm->isDefault
        ? emitJump(arm->location, armBlock)
        : emitBranch(arm->location, lowerMatchCondition(selectors, arm), armBlock,
                     nextBlock);
    switchTo(armBlock);
    lowerBlock(arm->body.get(), true);
    if (!block().terminated) emitJump(arm->location, exitBlock);
    switchTo(nextBlock);
    if (arm->isDefault) break;
  }
  switchTo(exitBlock);
}

Operand MIRBuilder::lowerMatchExpression(MatchExprNode *node) {
  Operand result = nextTemp();
  std::vector<Operand> selectors;
  for (auto &expr : node->expressions) selectors.push_back(lowerExpression(expr.get()));
  const std::size_t exitBlock = createBlock("match_expr_exit");
  for (std::size_t i = 0; i < node->arms.size(); ++i) {
    auto *arm = static_cast<MatchArmNode *>(node->arms[i].get());
    const std::size_t armBlock = createBlock("match_expr_arm");
    const bool last = i + 1 == node->arms.size();
    const std::size_t nextBlock =
        arm->isDefault || last ? exitBlock : createBlock("match_expr_test");
    arm->isDefault
        ? emitJump(arm->location, armBlock)
        : emitBranch(arm->location, lowerMatchCondition(selectors, arm), armBlock,
                     nextBlock);
    switchTo(armBlock);
    emit({InstKind::Copy, arm->location, result.text, "", {},
          {lowerExpression(arm->valueExpr.get())}});
    if (!block().terminated) emitJump(arm->location, exitBlock);
    switchTo(nextBlock);
    if (arm->isDefault) break;
  }
  switchTo(exitBlock);
  return result;
}

Operand MIRBuilder::lowerCondition(ASTNode *node) {
  return node != nullptr ? lowerExpression(node) : constantTemp({}, "true");
}

Operand MIRBuilder::lowerExpression(ASTNode *node) {
  if (node == nullptr) return constantTemp({}, "void");
  switch (node->type) {
  case ASTNodeType::IntLiteral: return constantTemp(node->location, std::to_string(static_cast<IntLiteralNode *>(node)->value));
  case ASTNodeType::FloatLiteral: return constantTemp(node->location, std::to_string(static_cast<FloatLiteralNode *>(node)->value));
  case ASTNodeType::StringLiteral: return constantTemp(node->location, quote(static_cast<StringLiteralNode *>(node)->value));
  case ASTNodeType::BoolLiteral: return constantTemp(node->location, static_cast<BoolLiteralNode *>(node)->value ? "true" : "false");
  case ASTNodeType::NullLiteral: return constantTemp(node->location, "null");
  case ASTNodeType::Identifier:
    return copyTemp(node->location,
                    resolveName(static_cast<IdentifierNode *>(node)->name));
  case ASTNodeType::BinaryExpr: {
    Operand result = nextTemp();
    auto *binary = static_cast<BinaryExprNode *>(node);
    emit({InstKind::Binary, node->location, result.text, tokenText(binary->op), {},
          {lowerExpression(binary->left.get()), lowerExpression(binary->right.get())}});
    return result;
  }
  case ASTNodeType::UnaryExpr: {
    Operand result = nextTemp();
    auto *unary = static_cast<UnaryExprNode *>(node);
    if (unary->op == TokenType::ValueOf) {
      emit({InstKind::Deref, node->location, result.text, "", {},
            {lowerExpression(unary->operand.get())}});
      return result;
    }
    emit({InstKind::Unary, node->location, result.text, tokenText(unary->op), {},
          {lowerExpression(unary->operand.get())}});
    return result;
  }
  case ASTNodeType::CallExpr: {
    auto *call = static_cast<CallExprNode *>(node);
    std::vector<Operand> arguments;
    for (auto &argument : call->arguments) arguments.push_back(lowerExpression(argument.get()));
    Operand result = nextTemp();
    Instruction inst{InstKind::Call, node->location, result.text, "",
                     lowerExpression(call->callee.get()), std::move(arguments)};
    for (std::size_t i = 0; i < call->fusionCallNames.size(); ++i) {
      if (i != 0) inst.note += " -> ";
      inst.note += call->fusionCallNames[i];
    }
    emit(std::move(inst));
    return result;
  }
  case ASTNodeType::MemberAccessExpr: {
    auto *member = static_cast<MemberAccessNode *>(node);
    Operand result = nextTemp();
    emit({InstKind::Member, node->location, result.text, member->member, {},
          {lowerExpression(member->object.get())}});
    return result;
  }
  case ASTNodeType::IndexExpr: {
    auto *index = static_cast<IndexExprNode *>(node);
    std::vector<Operand> operands{lowerExpression(index->object.get())};
    for (auto &item : index->indices) operands.push_back(lowerExpression(item.get()));
    Operand result = nextTemp();
    emit({InstKind::Index, node->location, result.text, "", {}, std::move(operands)});
    return result;
  }
  case ASTNodeType::SliceExpr: {
    auto *slice = static_cast<SliceExprNode *>(node);
    Operand result = nextTemp();
    emit({InstKind::Slice, node->location, result.text, "", {},
          {lowerExpression(slice->object.get()), lowerExpression(slice->start.get()),
           lowerExpression(slice->end.get())}});
    return result;
  }
  case ASTNodeType::TypeofExpr: {
    Operand result = nextTemp();
    emit({InstKind::Typeof, node->location, result.text, "", {},
          {lowerExpression(static_cast<TypeofExprNode *>(node)->expression.get())}});
    return result;
  }
  case ASTNodeType::MatchExpr: return lowerMatchExpression(static_cast<MatchExprNode *>(node));
  default: return unsupportedExpr(node, "unsupported expr: " + describeNode(node));
  }
}

} // namespace neuron::mir
