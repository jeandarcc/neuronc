#pragma once

#include "AnalysisContext.h"
#include "BindingAnalyzer.h"
#include "CallableBinder.h"
#include "DeclarationAnalyzer.h"
#include "ExpressionAnalyzer.h"
#include "FlowAnalyzer.h"
#include "GraphicsAnalyzer.h"
#include "InputAnalyzer.h"
#include "RuleValidator.h"
#include "StatementAnalyzer.h"
#include "TypeChecker.h"

namespace neuron::sema_detail {

class SemanticDriver {
public:
  SemanticDriver(const AnalysisOptions &options, SymbolTable &symbols,
                 TypeResolver &types, ReferenceTracker &references,
                 ScopeManager &scopes, DiagnosticEmitter &diagnostics,
                 const TypeChecker &typeChecker);

  void analyze(const ProgramView &program);
  void analyze(ProgramNode *program);

  AnalysisContext &context();
  const AnalysisContext &context() const;
  const TypeChecker &typeChecker() const;
  RuleValidator &rules();
  CallableBinder &binder();
  InputAnalyzer &input();
  ExpressionAnalyzer &expressions();
  FlowAnalyzer &flow();
  BindingAnalyzer &bindings();
  StatementAnalyzer &statements();
  GraphicsAnalyzer &graphics();
  DeclarationAnalyzer &declarations();

  void visitMethodDecl(MethodDeclNode *node);
  void visitBlock(BlockNode *node);
  void visitDeclarations(const std::vector<ASTNode *> &declarations);
  NTypePtr inferExpression(ASTNode *expr);

private:
  AnalysisContext m_context;
  FlowAnalyzer m_flow;
  const TypeChecker &m_typeChecker;
  RuleValidator m_rules;
  CallableBinder m_binder;
  InputAnalyzer m_input;
  ExpressionAnalyzer m_expressions;
  BindingAnalyzer m_bindings;
  StatementAnalyzer m_statements;
  GraphicsAnalyzer m_graphics;
  DeclarationAnalyzer m_declarations;
};

} // namespace neuron::sema_detail
