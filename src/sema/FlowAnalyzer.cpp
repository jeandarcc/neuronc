#include "FlowAnalyzer.h"

#include <utility>

namespace neuron::sema_detail {

namespace {

bool isNullLiteralType(const NTypePtr &type) {
  return type && type->isNullable() && type->nullableBase() != nullptr &&
         type->nullableBase()->isUnknown();
}

} // namespace

FlowAnalyzer::FlowAnalyzer(AnalysisContext &context) : m_context(context) {
  reset();
}

void FlowAnalyzer::reset() {
  m_snapshot = {};
  m_snapshot.reachable = true;
  m_snapshot.scopeStack.clear();
  m_snapshot.scopeStack.emplace_back();
}

void FlowAnalyzer::enterScope() { m_snapshot.scopeStack.emplace_back(); }

void FlowAnalyzer::leaveScope() {
  if (m_snapshot.scopeStack.empty()) {
    return;
  }

  for (const Symbol *symbol : m_snapshot.scopeStack.back()) {
    m_snapshot.variables.erase(symbol);
  }
  m_snapshot.scopeStack.pop_back();
  if (m_snapshot.scopeStack.empty()) {
    m_snapshot.scopeStack.emplace_back();
  }
}

FlowAnalyzer::Snapshot FlowAnalyzer::snapshot() const { return m_snapshot; }

void FlowAnalyzer::restore(Snapshot snapshot) {
  m_snapshot = std::move(snapshot);
  if (m_snapshot.scopeStack.empty()) {
    m_snapshot.scopeStack.emplace_back();
  }
}

FlowAnalyzer::Snapshot FlowAnalyzer::merge(const Snapshot &lhs,
                                           const Snapshot &rhs) const {
  if (!lhs.reachable) {
    return rhs;
  }
  if (!rhs.reachable) {
    return lhs;
  }

  Snapshot merged = lhs;
  merged.reachable = true;
  for (const auto &[symbol, rhsState] : rhs.variables) {
    auto it = merged.variables.find(symbol);
    if (it == merged.variables.end()) {
      merged.variables.emplace(symbol, rhsState);
      continue;
    }
    it->second = mergeVariableState(it->second, rhsState);
  }
  return merged;
}

bool FlowAnalyzer::isReachable() const { return m_snapshot.reachable; }

void FlowAnalyzer::markTerminated() { m_snapshot.reachable = false; }

void FlowAnalyzer::markReachable() { m_snapshot.reachable = true; }

void FlowAnalyzer::declareSymbol(Symbol *symbol, bool initialized,
                                 ASTNode *initialValue,
                                 const NTypePtr &valueType) {
  if (symbol == nullptr) {
    return;
  }

  if (m_snapshot.scopeStack.empty()) {
    m_snapshot.scopeStack.emplace_back();
  }
  m_snapshot.scopeStack.back().push_back(symbol);

  VariableState state;
  state.initState = initialized ? InitState::Initialized
                                : InitState::Uninitialized;
  state.nullState = initialized ? classifyValue(initialValue, valueType)
                                : NullState::Unknown;
  m_snapshot.variables[symbol] = state;
}

void FlowAnalyzer::assignSymbol(Symbol *symbol, ASTNode *value,
                                const NTypePtr &valueType) {
  VariableState *state = findState(symbol);
  if (state == nullptr) {
    return;
  }

  state->initState = InitState::Initialized;
  state->nullState = classifyValue(value, valueType);
}

void FlowAnalyzer::validateRead(Symbol *symbol, const SourceLocation &loc) {
  VariableState *state = findState(symbol);
  if (state == nullptr) {
    return;
  }

  switch (state->initState) {
  case InitState::Uninitialized:
    if (!state->reportedUninitializedUse) {
      m_context.error(loc, "N2204", {{"name", symbol->name}},
                      "Variable is used before it is initialized: " +
                          symbol->name);
      state->reportedUninitializedUse = true;
    }
    break;
  case InitState::MaybeUninitialized:
    if (!state->reportedMaybeUninitializedUse) {
      m_context.error(loc,
                      "Variable may be uninitialized when used: " +
                          symbol->name);
      state->reportedMaybeUninitializedUse = true;
    }
    break;
  case InitState::Initialized:
    break;
  }
}

bool FlowAnalyzer::validateNonNull(Symbol *symbol, const SourceLocation &loc,
                                   std::string_view usage,
                                   const NTypePtr &exprType) {
  VariableState *state = findState(symbol);
  if (state == nullptr) {
    if (exprType && exprType->isNullable() && !isNullLiteralType(exprType)) {
      m_context.error(loc, "Value of type '" + exprType->toString() +
                               "' may be null when used in " +
                               std::string(usage));
      return false;
    }
    return true;
  }

  switch (state->nullState) {
  case NullState::Null:
    if (!state->reportedNullUse) {
      m_context.error(loc, "Null value used in " + std::string(usage) +
                               ": " + symbol->name);
      state->reportedNullUse = true;
    }
    return false;
  case NullState::MaybeNull:
    if (!state->reportedMaybeNullUse) {
      m_context.error(loc, "Value may be null when used in " +
                               std::string(usage) + ": " + symbol->name);
      state->reportedMaybeNullUse = true;
    }
    return false;
  case NullState::Unknown:
    if (exprType && exprType->isNullable() && !isNullLiteralType(exprType)) {
      m_context.error(loc, "Value of type '" + exprType->toString() +
                               "' may be null when used in " +
                               std::string(usage));
      return false;
    }
    return true;
  case NullState::NonNull:
    return true;
  }

  return true;
}

bool FlowAnalyzer::validateNonNullExpression(ASTNode *expr,
                                             const NTypePtr &exprType,
                                             const SourceLocation &loc,
                                             std::string_view usage) {
  if (expr == nullptr) {
    return true;
  }
  if (expr->type == ASTNodeType::NullLiteral) {
    m_context.error(loc, "Null value used in " + std::string(usage));
    return false;
  }
  if (exprType && exprType->isError()) {
    return false;
  }

  if (expr->type == ASTNodeType::Identifier) {
    auto *identifier = static_cast<IdentifierNode *>(expr);
    if (Symbol *symbol = m_context.currentScope()->lookup(identifier->name)) {
      return validateNonNull(symbol, loc, usage, exprType);
    }
  }

  if (exprType && exprType->isNullable() && !isNullLiteralType(exprType)) {
    m_context.error(loc, "Value of type '" + exprType->toString() +
                             "' may be null when used in " +
                             std::string(usage));
    return false;
  }
  return true;
}

void FlowAnalyzer::refineCondition(ASTNode *condition, bool assumeTrue) {
  if (condition == nullptr || condition->type != ASTNodeType::BinaryExpr) {
    return;
  }

  auto *binary = static_cast<BinaryExprNode *>(condition);
  if (binary->op != TokenType::EqualEqual && binary->op != TokenType::NotEqual) {
    return;
  }

  IdentifierNode *identifier = nullptr;
  ASTNode *otherSide = nullptr;
  if (binary->left != nullptr && binary->right != nullptr &&
      binary->left->type == ASTNodeType::Identifier &&
      binary->right->type == ASTNodeType::NullLiteral) {
    identifier = static_cast<IdentifierNode *>(binary->left.get());
    otherSide = binary->right.get();
  } else if (binary->left != nullptr && binary->right != nullptr &&
             binary->right->type == ASTNodeType::Identifier &&
             binary->left->type == ASTNodeType::NullLiteral) {
    identifier = static_cast<IdentifierNode *>(binary->right.get());
    otherSide = binary->left.get();
  }

  if (identifier == nullptr || otherSide == nullptr ||
      otherSide->type != ASTNodeType::NullLiteral) {
    return;
  }

  Symbol *symbol = m_context.currentScope()->lookup(identifier->name);
  if (symbol == nullptr) {
    return;
  }

  const bool matchesNull =
      (binary->op == TokenType::EqualEqual && assumeTrue) ||
      (binary->op == TokenType::NotEqual && !assumeTrue);
  setNullState(symbol, matchesNull ? NullState::Null : NullState::NonNull);
}

FlowAnalyzer::VariableState *FlowAnalyzer::findState(Symbol *symbol) {
  auto it = m_snapshot.variables.find(symbol);
  return it != m_snapshot.variables.end() ? &it->second : nullptr;
}

const FlowAnalyzer::VariableState *
FlowAnalyzer::findState(const Symbol *symbol) const {
  auto it = m_snapshot.variables.find(symbol);
  return it != m_snapshot.variables.end() ? &it->second : nullptr;
}

FlowAnalyzer::NullState FlowAnalyzer::classifyValue(
    ASTNode *value, const NTypePtr &valueType) const {
  if (value == nullptr) {
    return NullState::Unknown;
  }
  if (value->type == ASTNodeType::NullLiteral) {
    return NullState::Null;
  }
  if (value->type == ASTNodeType::IntLiteral ||
      value->type == ASTNodeType::FloatLiteral ||
      value->type == ASTNodeType::StringLiteral ||
      value->type == ASTNodeType::BoolLiteral ||
      value->type == ASTNodeType::TypeofExpr) {
    return NullState::NonNull;
  }

  if (value->type == ASTNodeType::Identifier) {
    auto *identifier = static_cast<IdentifierNode *>(value);
    if (const Symbol *source = m_context.currentScope()->lookup(identifier->name)) {
      if (const VariableState *state = findState(source)) {
        return state->nullState;
      }
    }
  }

  if (valueType == nullptr || valueType->isUnknown() || valueType->isDynamic()) {
    return NullState::Unknown;
  }
  return valueType->isNullable() ? NullState::MaybeNull : NullState::NonNull;
}

void FlowAnalyzer::setNullState(Symbol *symbol, NullState state) {
  if (VariableState *varState = findState(symbol)) {
    varState->nullState = state;
  }
}

FlowAnalyzer::VariableState
FlowAnalyzer::mergeVariableState(const VariableState &lhs,
                                 const VariableState &rhs) {
  VariableState merged;
  merged.initState = mergeInitState(lhs.initState, rhs.initState);
  merged.nullState = mergeNullState(lhs.nullState, rhs.nullState);
  merged.reportedUninitializedUse =
      lhs.reportedUninitializedUse || rhs.reportedUninitializedUse;
  merged.reportedMaybeUninitializedUse =
      lhs.reportedMaybeUninitializedUse || rhs.reportedMaybeUninitializedUse;
  merged.reportedNullUse = lhs.reportedNullUse || rhs.reportedNullUse;
  merged.reportedMaybeNullUse =
      lhs.reportedMaybeNullUse || rhs.reportedMaybeNullUse;
  return merged;
}

FlowAnalyzer::InitState FlowAnalyzer::mergeInitState(InitState lhs,
                                                     InitState rhs) {
  if (lhs == rhs) {
    return lhs;
  }
  if (lhs == InitState::Initialized && rhs == InitState::Initialized) {
    return InitState::Initialized;
  }
  if (lhs == InitState::Uninitialized && rhs == InitState::Uninitialized) {
    return InitState::Uninitialized;
  }
  return InitState::MaybeUninitialized;
}

FlowAnalyzer::NullState FlowAnalyzer::mergeNullState(NullState lhs,
                                                     NullState rhs) {
  if (lhs == rhs) {
    return lhs;
  }
  if (lhs == NullState::Unknown || rhs == NullState::Unknown) {
    return NullState::Unknown;
  }
  return NullState::MaybeNull;
}

} // namespace neuron::sema_detail
