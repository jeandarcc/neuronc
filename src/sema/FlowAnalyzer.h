#pragma once

#include "AnalysisContext.h"

#include <string_view>
#include <unordered_map>
#include <vector>

namespace neuron::sema_detail {

class FlowAnalyzer {
public:
  enum class InitState {
    Uninitialized,
    Initialized,
    MaybeUninitialized,
  };

  enum class NullState {
    Unknown,
    NonNull,
    Null,
    MaybeNull,
  };

  struct VariableState {
    InitState initState = InitState::Initialized;
    NullState nullState = NullState::Unknown;
    bool reportedUninitializedUse = false;
    bool reportedMaybeUninitializedUse = false;
    bool reportedNullUse = false;
    bool reportedMaybeNullUse = false;
  };

  struct Snapshot {
    bool reachable = true;
    std::unordered_map<const Symbol *, VariableState> variables;
    std::vector<std::vector<const Symbol *>> scopeStack;
  };

  explicit FlowAnalyzer(AnalysisContext &context);

  void reset();
  void enterScope();
  void leaveScope();

  Snapshot snapshot() const;
  void restore(Snapshot snapshot);
  Snapshot merge(const Snapshot &lhs, const Snapshot &rhs) const;

  bool isReachable() const;
  void markTerminated();
  void markReachable();

  void declareSymbol(Symbol *symbol, bool initialized, ASTNode *initialValue,
                     const NTypePtr &valueType = nullptr);
  void assignSymbol(Symbol *symbol, ASTNode *value,
                    const NTypePtr &valueType = nullptr);

  void validateRead(Symbol *symbol, const SourceLocation &loc);
  bool validateNonNull(Symbol *symbol, const SourceLocation &loc,
                       std::string_view usage,
                       const NTypePtr &exprType = nullptr);
  bool validateNonNullExpression(ASTNode *expr, const NTypePtr &exprType,
                                 const SourceLocation &loc,
                                 std::string_view usage);

  void refineCondition(ASTNode *condition, bool assumeTrue);

private:
  VariableState *findState(Symbol *symbol);
  const VariableState *findState(const Symbol *symbol) const;
  NullState classifyValue(ASTNode *value, const NTypePtr &valueType) const;
  void setNullState(Symbol *symbol, NullState state);
  static VariableState mergeVariableState(const VariableState &lhs,
                                          const VariableState &rhs);
  static InitState mergeInitState(InitState lhs, InitState rhs);
  static NullState mergeNullState(NullState lhs, NullState rhs);

  AnalysisContext &m_context;
  Snapshot m_snapshot;
};

} // namespace neuron::sema_detail
