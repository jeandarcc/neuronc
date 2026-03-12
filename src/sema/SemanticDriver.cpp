#include "SemanticDriver.h"

namespace neuron::sema_detail {

SemanticDriver::SemanticDriver(const AnalysisOptions &options,
                               SymbolTable &symbols, TypeResolver &types,
                               ReferenceTracker &references,
                               ScopeManager &scopes,
                               DiagnosticEmitter &diagnostics,
                               const TypeChecker &typeChecker)
    : m_context(options, symbols, types, references, scopes, diagnostics),
      m_flow(m_context),
      m_typeChecker(typeChecker), m_rules(*this), m_binder(*this),
      m_input(*this), m_expressions(*this), m_bindings(*this),
      m_statements(*this), m_graphics(*this), m_declarations(*this) {}

void SemanticDriver::analyze(const ProgramView &program) {
  m_declarations.analyze(program);
}

void SemanticDriver::analyze(ProgramNode *program) {
  if (program == nullptr) {
    return;
  }
  std::vector<ASTNode *> declarations;
  declarations.reserve(program->declarations.size());
  for (auto &decl : program->declarations) {
    declarations.push_back(decl.get());
  }
  analyze({program->location, program->moduleName, std::move(declarations)});
}

AnalysisContext &SemanticDriver::context() { return m_context; }

const AnalysisContext &SemanticDriver::context() const { return m_context; }

const TypeChecker &SemanticDriver::typeChecker() const { return m_typeChecker; }

RuleValidator &SemanticDriver::rules() { return m_rules; }

CallableBinder &SemanticDriver::binder() { return m_binder; }

InputAnalyzer &SemanticDriver::input() { return m_input; }

ExpressionAnalyzer &SemanticDriver::expressions() { return m_expressions; }

FlowAnalyzer &SemanticDriver::flow() { return m_flow; }

BindingAnalyzer &SemanticDriver::bindings() { return m_bindings; }

StatementAnalyzer &SemanticDriver::statements() { return m_statements; }

GraphicsAnalyzer &SemanticDriver::graphics() { return m_graphics; }

DeclarationAnalyzer &SemanticDriver::declarations() { return m_declarations; }

void SemanticDriver::visitMethodDecl(MethodDeclNode *node) {
  m_declarations.visitMethodDecl(node);
}

void SemanticDriver::visitBlock(BlockNode *node) { m_statements.visitBlock(node); }

void SemanticDriver::visitDeclarations(
    const std::vector<ASTNode *> &declarations) {
  m_declarations.visitDeclarations(declarations);
}

NTypePtr SemanticDriver::inferExpression(ASTNode *expr) {
  return m_expressions.infer(expr);
}

} // namespace neuron::sema_detail
