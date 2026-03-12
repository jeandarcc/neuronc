#include "neuronc/sema/SemanticAnalyzer.h"

#include "AnalysisContext.h"
#include "AnalysisOptions.h"
#include "DiagnosticEmitter.h"
#include "ReferenceTracker.h"
#include "SemanticDriver.h"
#include "ScopeManager.h"
#include "SymbolTable.h"
#include "TypeChecker.h"
#include "TypeResolver.h"

#include <memory>

namespace neuron {

SemanticAnalyzer::SemanticAnalyzer()
    : m_diagnostics(std::make_unique<sema_detail::DiagnosticEmitter>()),
      m_referenceTracker(std::make_unique<sema_detail::ReferenceTracker>()),
      m_scopeManager(std::make_unique<sema_detail::ScopeManager>()),
      m_symbolTable(std::make_unique<sema_detail::SymbolTable>()),
      m_typeChecker(std::make_unique<sema_detail::TypeChecker>()),
      m_typeResolver(std::make_unique<sema_detail::TypeResolver>()) {
  resetAnalysisState();
}

SemanticAnalyzer::~SemanticAnalyzer() = default;

void SemanticAnalyzer::resetAnalysisState() {
  sema_detail::AnalysisOptions options;
  options.maxClassesPerFile = m_maxClassesPerFile;
  options.requireMethodUppercaseStart = m_requireMethodUppercaseStart;
  options.enforceStrictFileNamingRules = m_enforceStrictFileNamingRules;
  options.sourceFileStem = m_sourceFileStem;
  options.maxLinesPerMethod = m_maxLinesPerMethod;
  options.maxLinesPerBlockStatement = m_maxLinesPerBlockStatement;
  options.minMethodNameLength = m_minMethodNameLength;
  options.requireClassExplicitVisibility = m_requireClassExplicitVisibility;
  options.requirePropertyExplicitVisibility =
      m_requirePropertyExplicitVisibility;
  options.requireConstUppercase = m_requireConstUppercase;
  options.maxNestingDepth = m_maxNestingDepth;
  options.requirePublicMethodDocs = m_requirePublicMethodDocs;

  sema_detail::AnalysisContext context(options, *m_symbolTable, *m_typeResolver,
                                       *m_referenceTracker, *m_scopeManager,
                                       *m_diagnostics);
  context.reset();
}

void SemanticAnalyzer::analyze(ProgramNode *program) {
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

void SemanticAnalyzer::analyze(const ProgramView &program) {
  sema_detail::AnalysisOptions options;
  options.maxClassesPerFile = m_maxClassesPerFile;
  options.requireMethodUppercaseStart = m_requireMethodUppercaseStart;
  options.enforceStrictFileNamingRules = m_enforceStrictFileNamingRules;
  options.sourceFileStem = m_sourceFileStem;
  options.maxLinesPerMethod = m_maxLinesPerMethod;
  options.maxLinesPerBlockStatement = m_maxLinesPerBlockStatement;
  options.minMethodNameLength = m_minMethodNameLength;
  options.requireClassExplicitVisibility = m_requireClassExplicitVisibility;
  options.requirePropertyExplicitVisibility =
      m_requirePropertyExplicitVisibility;
  options.requireConstUppercase = m_requireConstUppercase;
  options.maxNestingDepth = m_maxNestingDepth;
  options.requirePublicMethodDocs = m_requirePublicMethodDocs;

  sema_detail::SemanticDriver driver(options, *m_symbolTable, *m_typeResolver,
                                     *m_referenceTracker, *m_scopeManager,
                                     *m_diagnostics, *m_typeChecker);
  driver.analyze(program);
}

void SemanticAnalyzer::setAvailableModules(
    const std::unordered_set<std::string> &modules, bool enforceResolution) {
  m_typeResolver->setAvailableModules(modules, enforceResolution);
}

void SemanticAnalyzer::setMaxClassesPerFile(int maxClasses) {
  m_maxClassesPerFile = maxClasses > 0 ? maxClasses : 0;
}

void SemanticAnalyzer::setSingleClassPerFile(bool enforceRule) {
  m_maxClassesPerFile = enforceRule ? 1 : 0;
}

void SemanticAnalyzer::setRequireMethodUppercaseStart(
    bool requireUppercaseStart) {
  m_requireMethodUppercaseStart = requireUppercaseStart;
}

void SemanticAnalyzer::setStrictFileNamingRules(
    bool enforceRules, const std::string &sourceFileStem) {
  m_enforceStrictFileNamingRules = enforceRules;
  m_sourceFileStem = sourceFileStem;
}

void SemanticAnalyzer::setMaxLinesPerMethod(int maxLines) {
  m_maxLinesPerMethod = maxLines > 0 ? maxLines : 0;
}

void SemanticAnalyzer::setMaxLinesPerBlockStatement(int maxLines) {
  m_maxLinesPerBlockStatement = maxLines > 0 ? maxLines : 0;
}

void SemanticAnalyzer::setMinMethodNameLength(int minLength) {
  m_minMethodNameLength = minLength > 0 ? minLength : 0;
}

void SemanticAnalyzer::setRequireClassExplicitVisibility(
    bool requireExplicitVisibility) {
  m_requireClassExplicitVisibility = requireExplicitVisibility;
}

void SemanticAnalyzer::setRequirePropertyExplicitVisibility(
    bool requireExplicitVisibility) {
  m_requirePropertyExplicitVisibility = requireExplicitVisibility;
}

void SemanticAnalyzer::setRequireConstUppercase(bool requireConstUppercase) {
  m_requireConstUppercase = requireConstUppercase;
}

void SemanticAnalyzer::setMaxNestingDepth(int maxDepth) {
  m_maxNestingDepth = maxDepth > 0 ? maxDepth : 0;
}

void SemanticAnalyzer::setRequirePublicMethodDocs(bool requireDocs) {
  m_requirePublicMethodDocs = requireDocs;
}

void SemanticAnalyzer::setSourceText(std::string sourceText) {
  m_diagnostics->setSourceText(std::move(sourceText));
}

void SemanticAnalyzer::setAgentHints(bool enabled) {
  m_diagnostics->setAgentHints(enabled);
}

void SemanticAnalyzer::setModuleCppModules(
    const std::unordered_map<std::string, NativeModuleInfo> &modules) {
  m_typeResolver->setModuleCppModules(modules);
}

const std::vector<SemanticError> &SemanticAnalyzer::getErrors() const {
  return m_diagnostics->errors();
}

bool SemanticAnalyzer::hasErrors() const { return m_diagnostics->hasErrors(); }

NTypePtr SemanticAnalyzer::getInferredType(const ASTNode *node) const {
  return m_referenceTracker->inferredType(node);
}

std::optional<SymbolLocation> SemanticAnalyzer::getDefinitionLocation(
    const SourceLocation &referenceLocation) const {
  return m_referenceTracker->definitionLocation(referenceLocation);
}

std::vector<SymbolLocation> SemanticAnalyzer::getReferenceLocations(
    const SourceLocation &referenceLocation) const {
  return m_referenceTracker->referenceLocations(referenceLocation);
}

std::vector<DocumentSymbolInfo> SemanticAnalyzer::getDocumentSymbols() const {
  return m_referenceTracker->documentSymbols();
}

std::optional<VisibleSymbolInfo> SemanticAnalyzer::getResolvedSymbol(
    const SourceLocation &location) const {
  return m_referenceTracker->resolvedSymbol(location);
}

std::vector<VisibleSymbolInfo> SemanticAnalyzer::getScopeSnapshot(
    const SourceLocation &location) const {
  return m_scopeManager->snapshotAt(location);
}

std::vector<VisibleSymbolInfo>
SemanticAnalyzer::getTypeMembers(const NTypePtr &type) const {
  return m_typeResolver->getTypeMembers(type, *m_symbolTable);
}

std::vector<CallableSignatureInfo> SemanticAnalyzer::getCallableSignatures(
    std::string_view callableKey) const {
  return m_typeResolver->getCallableSignatures(callableKey);
}

} // namespace neuron
