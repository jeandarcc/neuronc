#include "StatementAnalyzer.h"

#include "AnalysisHelpers.h"
#include "SemanticDriver.h"

#include <algorithm>

namespace neuron::sema_detail {

namespace {

class ControlDepthGuard {
public:
  explicit ControlDepthGuard(AnalysisContext &context) : m_context(context) {
    m_context.enterControl();
  }

  ~ControlDepthGuard() { m_context.leaveControl(); }

private:
  AnalysisContext &m_context;
};

void declareLocalMethod(SemanticDriver &driver, MethodDeclNode *method) {
  AnalysisContext &context = driver.context();
  NTypePtr returnType = context.resolveType(method->returnType.get());
  std::vector<NTypePtr> parameterTypes;
  std::vector<std::string> parameterNames;
  std::vector<CallableParameterInfo> signatureParameters;

  parameterTypes.reserve(method->parameters.size());
  parameterNames.reserve(method->parameters.size());
  signatureParameters.reserve(method->parameters.size());
  for (const auto &param : method->parameters) {
    NTypePtr paramType = context.resolveType(param.typeSpec.get());
    parameterTypes.push_back(paramType);
    parameterNames.push_back(param.name);
    signatureParameters.push_back({param.name, typeDisplayName(paramType)});
  }

  Symbol methodSymbol(method->name, SymbolKind::Method,
                      NType::makeMethod(returnType, parameterTypes));
  methodSymbol.signatureKey = method->name;
  context.defineSymbol(context.currentScope(), method->name,
                       std::move(methodSymbol), &method->location,
                       symbolNameLength(method->name));
  context.registerCallableParamNames(method->name, std::move(parameterNames));
  context.registerCallableSignature(method->name,
                                    std::move(signatureParameters),
                                    typeDisplayName(returnType));
}

} // namespace

StatementAnalyzer::StatementAnalyzer(SemanticDriver &driver)
    : m_driver(driver) {}

bool StatementAnalyzer::visitStatementNode(ASTNode *node) {
  if (node == nullptr) {
    return m_driver.flow().isReachable();
  }

  switch (node->type) {
  case ASTNodeType::BindingDecl:
    m_driver.bindings().visit(static_cast<BindingDeclNode *>(node));
    break;
  case ASTNodeType::IfStmt:
    visitIfStmt(static_cast<IfStmtNode *>(node));
    break;
  case ASTNodeType::MatchStmt:
    visitMatchStmt(static_cast<MatchStmtNode *>(node));
    break;
  case ASTNodeType::WhileStmt:
    visitWhileStmt(static_cast<WhileStmtNode *>(node));
    break;
  case ASTNodeType::ForStmt:
    visitForStmt(static_cast<ForStmtNode *>(node));
    break;
  case ASTNodeType::ForInStmt:
    visitForInStmt(static_cast<ForInStmtNode *>(node));
    break;
  case ASTNodeType::CastStmt:
    visitCastStmt(static_cast<CastStmtNode *>(node));
    break;
  case ASTNodeType::ReturnStmt:
    visitReturnStmt(static_cast<ReturnStmtNode *>(node));
    m_driver.flow().markTerminated();
    break;
  case ASTNodeType::MethodDecl:
    declareLocalMethod(m_driver, static_cast<MethodDeclNode *>(node));
    m_driver.visitMethodDecl(static_cast<MethodDeclNode *>(node));
    break;
  case ASTNodeType::TryStmt:
    visitTryStmt(static_cast<TryStmtNode *>(node));
    break;
  case ASTNodeType::ThrowStmt:
    visitThrowStmt(static_cast<ThrowStmtNode *>(node));
    m_driver.flow().markTerminated();
    break;
  case ASTNodeType::StaticAssertStmt:
    visitStaticAssertStmt(static_cast<StaticAssertStmtNode *>(node));
    break;
  case ASTNodeType::MacroDecl:
    visitMacroDecl(static_cast<MacroDeclNode *>(node));
    break;
  case ASTNodeType::GpuBlock:
    m_driver.graphics().visitGpuBlock(static_cast<GpuBlockNode *>(node));
    break;
  case ASTNodeType::CanvasStmt:
    m_driver.graphics().visitCanvasStmt(static_cast<CanvasStmtNode *>(node));
    break;
  case ASTNodeType::ShaderDecl:
    m_driver.graphics().visitShaderDecl(static_cast<ShaderDeclNode *>(node));
    break;
  case ASTNodeType::UnsafeBlock: {
    auto *unsafeBlock = static_cast<UnsafeBlockNode *>(node);
    visitStatementLike(unsafeBlock->body.get());
    break;
  }
  case ASTNodeType::BreakStmt:
  case ASTNodeType::ContinueStmt:
    m_driver.flow().markTerminated();
    break;
  case ASTNodeType::ShaderPassStmt:
    break;
  case ASTNodeType::CallExpr:
  case ASTNodeType::IncrementStmt:
  case ASTNodeType::DecrementStmt:
  case ASTNodeType::BinaryExpr:
    m_driver.inferExpression(node);
    break;
  default:
    break;
  }

  return m_driver.flow().isReachable();
}

bool StatementAnalyzer::visitStatementLike(ASTNode *node) {
  if (node == nullptr) {
    return m_driver.flow().isReachable();
  }
  if (node->type == ASTNodeType::Block) {
    visitBlock(static_cast<BlockNode *>(node));
    return m_driver.flow().isReachable();
  }
  return visitStatementNode(node);
}

std::optional<bool> StatementAnalyzer::evaluateConstantBool(ASTNode *node) const {
  if (node == nullptr) {
    return std::nullopt;
  }
  switch (node->type) {
  case ASTNodeType::BoolLiteral:
    return static_cast<BoolLiteralNode *>(node)->value;
  case ASTNodeType::UnaryExpr: {
    auto *unary = static_cast<UnaryExprNode *>(node);
    if (unary->op != TokenType::Not) {
      return std::nullopt;
    }
    if (const std::optional<bool> operand =
            evaluateConstantBool(unary->operand.get())) {
      return !*operand;
    }
    return std::nullopt;
  }
  case ASTNodeType::BinaryExpr: {
    auto *binary = static_cast<BinaryExprNode *>(node);
    const std::optional<bool> left = evaluateConstantBool(binary->left.get());
    const std::optional<bool> right = evaluateConstantBool(binary->right.get());
    if (left.has_value() && right.has_value()) {
      switch (binary->op) {
      case TokenType::And:
        return *left && *right;
      case TokenType::Or:
        return *left || *right;
      case TokenType::EqualEqual:
        return *left == *right;
      case TokenType::NotEqual:
        return *left != *right;
      default:
        break;
      }
    }
    if (binary->left != nullptr && binary->right != nullptr &&
        binary->left->type == ASTNodeType::NullLiteral &&
        binary->right->type == ASTNodeType::NullLiteral) {
      if (binary->op == TokenType::EqualEqual) {
        return true;
      }
      if (binary->op == TokenType::NotEqual) {
        return false;
      }
    }
    return std::nullopt;
  }
  default:
    return std::nullopt;
  }
}

void StatementAnalyzer::visitBlock(BlockNode *node) {
  if (node == nullptr) {
    return;
  }

  AnalysisContext &context = m_driver.context();
  m_driver.rules().validateBlockLength(node, node->location, "",
                                       "block statement");
  context.enterScope("block");
  m_driver.flow().enterScope();
  context.recordScopeSnapshot(node->location);

  for (auto &stmt : node->statements) {
    if (!m_driver.flow().isReachable()) {
      context.error(stmt->location, "Unreachable code");
      continue;
    }
    visitStatementNode(stmt.get());
  }

  m_driver.flow().leaveScope();
  context.leaveScope();
  context.recordScopeSnapshot(blockEndLocation(node));
}

void StatementAnalyzer::visitIfStmt(IfStmtNode *node) {
  if (node == nullptr) {
    return;
  }

  validateControlDepth(node->location, "if", "Flatten nested branches with "
                                          "guard clauses or helper methods.");
  ControlDepthGuard controlGuard(m_driver.context());

  NTypePtr conditionType = m_driver.inferExpression(node->condition.get());
  if (!conditionType->isBool() && !conditionType->isUnknown()) {
    m_driver.context().error(node->location,
                             "Condition must be a boolean expression");
  }

  const std::optional<bool> constantCondition =
      evaluateConstantBool(node->condition.get());
  const sema_detail::FlowAnalyzer::Snapshot before = m_driver.flow().snapshot();

  sema_detail::FlowAnalyzer::Snapshot thenState = before;
  if (constantCondition.has_value() && !*constantCondition) {
    thenState.reachable = false;
  } else {
    m_driver.flow().restore(before);
    m_driver.flow().refineCondition(node->condition.get(), true);
    visitStatementLike(node->thenBlock.get());
    thenState = m_driver.flow().snapshot();
  }

  sema_detail::FlowAnalyzer::Snapshot elseState = before;
  if (node->elseBlock != nullptr) {
    if (constantCondition.has_value() && *constantCondition) {
      elseState.reachable = false;
    } else {
      m_driver.flow().restore(before);
      m_driver.flow().refineCondition(node->condition.get(), false);
      visitStatementLike(node->elseBlock.get());
      elseState = m_driver.flow().snapshot();
    }
  }

  m_driver.flow().restore(m_driver.flow().merge(thenState, elseState));
}

void StatementAnalyzer::visitMatchStmt(MatchStmtNode *node) {
  if (node == nullptr || node->expressions.empty()) {
    m_driver.context().error(node ? node->location : SourceLocation{},
                             "match statement requires an expression");
    return;
  }

  validateControlDepth(node->location, "match",
                       "Extract nested match logic into helper methods.");
  ControlDepthGuard controlGuard(m_driver.context());

  std::vector<NTypePtr> matchTypes;
  matchTypes.reserve(node->expressions.size());
  for (const auto &expr : node->expressions) {
    matchTypes.push_back(m_driver.inferExpression(expr.get()));
  }

  bool sawDefault = false;
  AnalysisContext &context = m_driver.context();
  context.enterScope("match");
  m_driver.flow().enterScope();
  const sema_detail::FlowAnalyzer::Snapshot before = m_driver.flow().snapshot();
  for (auto &armNode : node->arms) {
    auto *matchArm = static_cast<MatchArmNode *>(armNode.get());
    if (matchArm->isDefault) {
      if (sawDefault) {
        context.error(matchArm->location,
                      "match can only contain one default arm");
      }
      sawDefault = true;
    } else {
      if (matchArm->patternExprs.size() != matchTypes.size()) {
        context.error(matchArm->location,
                      "Match arm pattern count mismatch: expected " +
                          std::to_string(matchTypes.size()) + " but got " +
                          std::to_string(matchArm->patternExprs.size()));
      }
      const std::size_t pairCount =
          std::min(matchTypes.size(), matchArm->patternExprs.size());
      for (std::size_t i = 0; i < pairCount; ++i) {
        NTypePtr armType = m_driver.inferExpression(matchArm->patternExprs[i].get());
        if (!matchTypes[i]->isUnknown() && !armType->isUnknown() &&
            !matchTypes[i]->isDynamic() && !armType->isDynamic() &&
            !matchTypes[i]->equals(*armType)) {
          context.error(matchArm->location,
                        "Match arm type mismatch: expected '" +
                            matchTypes[i]->toString() + "' but got '" +
                            armType->toString() + "'");
        }
      }
    }

    if (matchArm->body) {
      m_driver.flow().restore(before);
      visitStatementLike(matchArm->body.get());
    }
  }
  m_driver.flow().restore(before);
  m_driver.flow().leaveScope();
  context.leaveScope();
  context.recordScopeSnapshot(
      !node->arms.empty() ? nodeEndLocation(node->arms.back().get())
                          : node->location);
}

void StatementAnalyzer::visitCastStmt(CastStmtNode *node) {
  if (node == nullptr || node->target == nullptr || node->steps.empty()) {
    m_driver.context().error(node ? node->location : SourceLocation{},
                             "cast statement requires a target and at least one "
                             "step");
    return;
  }

  AnalysisContext &context = m_driver.context();
  Symbol *targetSymbol = nullptr;
  std::string targetName;
  const bool targetIsIdentifier =
      node->target->type == ASTNodeType::Identifier;
  if (targetIsIdentifier) {
    targetName = static_cast<IdentifierNode *>(node->target.get())->name;
    targetSymbol = context.currentScope()->lookup(targetName);
  }

  const bool simpleTypedDeclaration =
      targetIsIdentifier && targetSymbol == nullptr && node->steps.size() == 1;
  if (simpleTypedDeclaration) {
    NTypePtr declaredType = m_driver.typeChecker().applyCastPipeline(
        context, NType::makeUnknown(), node, nullptr);
    if (declaredType->isAuto() || declaredType->isUnknown()) {
      declaredType = NType::makeDynamic();
    }
    Symbol symbol(targetName, SymbolKind::Variable, declaredType);
    const SourceLocation *targetLocation =
        node->target != nullptr ? &node->target->location : &node->location;
    Symbol *defined = context.defineSymbol(context.currentScope(), targetName,
                                           std::move(symbol), targetLocation,
                                           symbolNameLength(targetName));
    if (defined == nullptr) {
      context.error(node->location,
                    "Variable already defined in scope: " + targetName);
    } else {
      m_driver.flow().declareSymbol(defined, false, nullptr, declaredType);
    }
    return;
  }

  if (targetIsIdentifier && targetSymbol != nullptr && targetSymbol->isConst) {
    context.error(node->location,
                  "Cannot assign to const variable: " + targetName);
  }
  if (targetIsIdentifier && targetSymbol != nullptr) {
    context.recordReference(targetSymbol, node->target->location,
                            symbolNameLength(targetName));
  }

  NTypePtr sourceType = m_driver.inferExpression(node->target.get());
  NTypePtr finalType = m_driver.typeChecker().applyCastPipeline(
      context, sourceType, node, node->target.get());

  if (targetIsIdentifier && targetSymbol != nullptr) {
    targetSymbol->type = finalType;
    m_driver.flow().assignSymbol(targetSymbol, nullptr, finalType);
  } else if (targetIsIdentifier) {
    context.error(node->location, "Undefined identifier: " + targetName);
  }
}

void StatementAnalyzer::visitWhileStmt(WhileStmtNode *node) {
  if (node == nullptr) {
    return;
  }

  validateControlDepth(node->location, "while",
                       "Reduce nested loops by extracting logic into helper "
                       "methods.");
  ControlDepthGuard controlGuard(m_driver.context());

  NTypePtr conditionType = m_driver.inferExpression(node->condition.get());
  if (!conditionType->isBool() && !conditionType->isUnknown()) {
    m_driver.context().error(node->location,
                             "Condition must be a boolean expression");
  }

  const std::optional<bool> constantCondition =
      evaluateConstantBool(node->condition.get());
  const sema_detail::FlowAnalyzer::Snapshot before = m_driver.flow().snapshot();
  if (constantCondition.has_value() && !*constantCondition) {
    m_driver.flow().restore(before);
    return;
  }

  m_driver.flow().refineCondition(node->condition.get(), true);
  visitStatementLike(node->body.get());
  m_driver.flow().restore(before);
}

void StatementAnalyzer::visitForStmt(ForStmtNode *node) {
  if (node == nullptr) {
    return;
  }

  validateControlDepth(node->location, "for",
                       "Reduce nested loops by extracting logic into helper "
                       "methods.");
  ControlDepthGuard controlGuard(m_driver.context());

  AnalysisContext &context = m_driver.context();
  context.enterScope("for");
  m_driver.flow().enterScope();
  if (node->init) {
    if (node->init->type == ASTNodeType::BindingDecl) {
      m_driver.bindings().visit(static_cast<BindingDeclNode *>(node->init.get()));
    } else if (node->init->type == ASTNodeType::CastStmt) {
      visitCastStmt(static_cast<CastStmtNode *>(node->init.get()));
    } else {
      m_driver.inferExpression(node->init.get());
    }
  }
  const sema_detail::FlowAnalyzer::Snapshot afterInit = m_driver.flow().snapshot();
  if (node->condition) {
    NTypePtr conditionType = m_driver.inferExpression(node->condition.get());
    if (!conditionType->isBool() && !conditionType->isUnknown()) {
      m_driver.context().error(node->location,
                               "Condition must be a boolean expression");
    }
  }
  if (node->increment) {
    m_driver.inferExpression(node->increment.get());
  }
  const std::optional<bool> constantCondition =
      node->condition != nullptr ? evaluateConstantBool(node->condition.get())
                                 : std::nullopt;
  if (constantCondition.has_value() && !*constantCondition) {
  } else {
    if (node->condition != nullptr) {
      m_driver.flow().refineCondition(node->condition.get(), true);
    }
    visitStatementLike(node->body.get());
  }
  m_driver.flow().restore(afterInit);
  m_driver.flow().leaveScope();
  context.leaveScope();
  context.recordScopeSnapshot(nodeEndLocation(node->body.get()));
}

void StatementAnalyzer::visitForInStmt(ForInStmtNode *node) {
  if (node == nullptr) {
    return;
  }

  validateControlDepth(node->location, "for-in",
                       "Reduce nested loops by extracting logic into helper "
                       "methods.");
  ControlDepthGuard controlGuard(m_driver.context());

  AnalysisContext &context = m_driver.context();
  context.enterScope("for_in");
  m_driver.flow().enterScope();

  NTypePtr iterableType = m_driver.inferExpression(node->iterable.get());
  NTypePtr elementType = NType::makeUnknown();
  if ((iterableType->kind == TypeKind::Array ||
       iterableType->kind == TypeKind::Tensor) &&
      !iterableType->genericArgs.empty()) {
    elementType = iterableType->genericArgs[0];
  }

  if (Symbol *loopSymbol = context.defineSymbol(
          context.currentScope(), node->variable,
          Symbol(node->variable, SymbolKind::Variable, elementType),
          &node->variableLocation, symbolNameLength(node->variable))) {
    m_driver.flow().declareSymbol(loopSymbol, true, nullptr, elementType);
  }
  visitStatementLike(node->body.get());
  m_driver.flow().leaveScope();
  context.leaveScope();
  context.recordScopeSnapshot(nodeEndLocation(node->body.get()));
}

void StatementAnalyzer::visitReturnStmt(ReturnStmtNode *node) {
  if (node != nullptr && node->value) {
    m_driver.inferExpression(node->value.get());
  }
}

void StatementAnalyzer::visitTryStmt(TryStmtNode *node) {
  if (node == nullptr) {
    return;
  }
  if (!node->tryBlock) {
    m_driver.context().error(node->location, "try statement requires a body");
    return;
  }

  validateControlDepth(node->location, "try",
                       "Refactor nested control flow into smaller methods or "
                       "early returns.");
  ControlDepthGuard controlGuard(m_driver.context());

  AnalysisContext &context = m_driver.context();
  const sema_detail::FlowAnalyzer::Snapshot before = m_driver.flow().snapshot();
  visitStatementLike(node->tryBlock.get());
  std::vector<sema_detail::FlowAnalyzer::Snapshot> reachableStates;
  if (m_driver.flow().isReachable()) {
    reachableStates.push_back(m_driver.flow().snapshot());
  }

  if (node->catchClauses.empty() && !node->finallyBlock) {
    context.error(node->location,
                  "try statement must include catch and/or finally");
  }

  for (auto &clause : node->catchClauses) {
    auto *catchNode = static_cast<CatchClauseNode *>(clause.get());
    m_driver.flow().restore(before);
    context.enterScope("catch");
    m_driver.flow().enterScope();

    NTypePtr errorType = NType::makeUnknown();
    if (catchNode->errorType) {
      errorType = context.resolveType(catchNode->errorType.get());
    }
    if (!catchNode->errorName.empty()) {
      if (Symbol *catchSymbol = context.defineSymbol(
              context.currentScope(), catchNode->errorName,
              Symbol(catchNode->errorName, SymbolKind::Variable, errorType),
              &catchNode->errorLocation,
              symbolNameLength(catchNode->errorName))) {
        m_driver.flow().declareSymbol(catchSymbol, true, nullptr, errorType);
      }
    }

    if (catchNode->body) {
      visitStatementLike(catchNode->body.get());
    }

    if (m_driver.flow().isReachable()) {
      reachableStates.push_back(m_driver.flow().snapshot());
    }
    m_driver.flow().leaveScope();
    context.leaveScope();
    context.recordScopeSnapshot(catchNode->body != nullptr
                                    ? nodeEndLocation(catchNode->body.get())
                                    : catchNode->location);
  }

  sema_detail::FlowAnalyzer::Snapshot merged = before;
  if (reachableStates.empty()) {
    merged = before;
    merged.reachable = false;
  } else {
    merged = reachableStates.front();
    for (std::size_t i = 1; i < reachableStates.size(); ++i) {
      merged = m_driver.flow().merge(merged, reachableStates[i]);
    }
  }

  m_driver.flow().restore(merged);
  if (node->finallyBlock) {
    sema_detail::FlowAnalyzer::Snapshot finallyStart = merged;
    finallyStart.reachable = true;
    m_driver.flow().restore(finallyStart);
    visitStatementLike(node->finallyBlock.get());
    sema_detail::FlowAnalyzer::Snapshot finallyState =
        m_driver.flow().snapshot();
    if (!merged.reachable && finallyState.reachable) {
      finallyState.reachable = false;
    }
    m_driver.flow().restore(finallyState);
  }
}

void StatementAnalyzer::visitThrowStmt(ThrowStmtNode *node) {
  if (node == nullptr || !node->value) {
    m_driver.context().error(node ? node->location : SourceLocation{},
                             "throw requires an error expression");
    return;
  }
  m_driver.inferExpression(node->value.get());
}

void StatementAnalyzer::visitMacroDecl(MacroDeclNode *node) { (void)node; }

void StatementAnalyzer::visitStaticAssertStmt(StaticAssertStmtNode *node) {
  if (node == nullptr || node->condition == nullptr) {
    m_driver.context().error(node ? node->location : SourceLocation{},
                             "static_assert requires a condition");
    return;
  }

  NTypePtr conditionType = m_driver.inferExpression(node->condition.get());
  if (!conditionType->isBool() && !conditionType->isUnknown()) {
    m_driver.context().error(node->location,
                             "static_assert condition must be boolean");
    return;
  }

  if (node->condition->type == ASTNodeType::BoolLiteral &&
      !static_cast<BoolLiteralNode *>(node->condition.get())->value) {
    if (node->message.empty()) {
      m_driver.context().error(node->location, "static_assert failed");
    } else {
      m_driver.context().error(node->location,
                               "static_assert failed: " + node->message);
    }
  }
}

void StatementAnalyzer::validateControlDepth(const SourceLocation &loc,
                                             const std::string &statementName,
                                             std::string_view hint) {
  const AnalysisOptions &options = m_driver.context().options();
  const int nextDepth = m_driver.context().controlDepth() + 1;
  if (options.maxNestingDepth > 0 && nextDepth > options.maxNestingDepth) {
    m_driver.context().errorWithAgentHint(
        loc,
        "Maximum nesting depth exceeded in '" + statementName +
            "' statement (depth " + std::to_string(nextDepth) + ", limit " +
            std::to_string(options.maxNestingDepth) + ").",
        hint);
  }
}

} // namespace neuron::sema_detail
