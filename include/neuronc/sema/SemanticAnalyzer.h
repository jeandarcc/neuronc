#pragma once
#include "neuronc/diagnostics/DiagnosticFormat.h"
#include "neuronc/parser/AST.h"
#include "neuronc/sema/TypeSystem.h"
#include <string>
#include <vector>
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <string_view>

namespace neuron {

namespace sema_detail {
class DiagnosticEmitter;
class ReferenceTracker;
class ScopeManager;
class SymbolTable;
class TypeChecker;
class TypeResolver;
} // namespace sema_detail

struct SemanticError {
    std::string message;
    std::string code;
    diagnostics::DiagnosticArguments arguments;
    SourceLocation location;

    std::string toString() const {
        return location.file + ":" + std::to_string(location.line) + ":" + 
               std::to_string(location.column) + ": semantic error: " + message;
    }
};

struct NativeModuleExportSignature {
    std::string symbolName;
    std::vector<std::string> parameterTypeNames;
    std::string returnTypeName = "void";
};

struct NativeModuleInfo {
    std::string name;
    std::unordered_map<std::string, NativeModuleExportSignature> exports;
};

struct ProgramView {
    SourceLocation location;
    std::string moduleName;
    std::vector<ASTNode*> declarations;
};

struct SymbolRange {
    SourceLocation start;
    SourceLocation end;
};

struct DocumentSymbolInfo {
    std::string name;
    SymbolKind kind = SymbolKind::Variable;
    SymbolRange range;
    SymbolRange selectionRange;
    std::vector<DocumentSymbolInfo> children;
};

struct VisibleSymbolInfo {
    std::string name;
    SymbolKind kind = SymbolKind::Variable;
    NTypePtr type;
    std::string signatureKey;
    bool isPublic = false;
    bool isConst = false;
    std::optional<SymbolLocation> definition;
};

struct CallableParameterInfo {
    std::string name;
    std::string typeName;
};

struct CallableSignatureInfo {
    std::string key;
    std::string label;
    std::string returnType;
    std::vector<CallableParameterInfo> parameters;
};

class SemanticAnalyzer {
public:
    SemanticAnalyzer();
    ~SemanticAnalyzer();

    void analyze(ProgramNode* program);
    void analyze(const ProgramView& program);
    void setAvailableModules(const std::unordered_set<std::string>& modules,
                             bool enforceResolution);
    void setMaxClassesPerFile(int maxClasses);
    void setSingleClassPerFile(bool enforceRule);
    void setRequireMethodUppercaseStart(bool requireUppercaseStart);
    void setStrictFileNamingRules(bool enforceRules,
                                  const std::string& sourceFileStem);
    void setMaxLinesPerMethod(int maxLines);
    void setMaxLinesPerBlockStatement(int maxLines);
    void setMinMethodNameLength(int minLength);
    void setRequireClassExplicitVisibility(bool requireExplicitVisibility);
    void setRequirePropertyExplicitVisibility(bool requireExplicitVisibility);
    void setRequireConstUppercase(bool requireConstUppercase);
    void setMaxNestingDepth(int maxDepth);
    void setRequirePublicMethodDocs(bool requireDocs);
    void setSourceText(std::string sourceText);
    void setAgentHints(bool enabled);
    void setModuleCppModules(
        const std::unordered_map<std::string, NativeModuleInfo>& modules);

    const std::vector<SemanticError>& getErrors() const;
    bool hasErrors() const;
    NTypePtr getInferredType(const ASTNode* node) const;
    std::optional<SymbolLocation> getDefinitionLocation(
        const SourceLocation& referenceLocation) const;
    std::vector<SymbolLocation> getReferenceLocations(
        const SourceLocation& referenceLocation) const;
    std::vector<DocumentSymbolInfo> getDocumentSymbols() const;
    std::optional<VisibleSymbolInfo> getResolvedSymbol(
        const SourceLocation& location) const;
    std::vector<VisibleSymbolInfo> getScopeSnapshot(
        const SourceLocation& location) const;
    std::vector<VisibleSymbolInfo> getTypeMembers(const NTypePtr& type) const;
    std::vector<CallableSignatureInfo> getCallableSignatures(
        std::string_view callableKey) const;

private:
    void resetAnalysisState();

    std::unique_ptr<sema_detail::DiagnosticEmitter> m_diagnostics;
    std::unique_ptr<sema_detail::ReferenceTracker> m_referenceTracker;
    std::unique_ptr<sema_detail::ScopeManager> m_scopeManager;
    std::unique_ptr<sema_detail::SymbolTable> m_symbolTable;
    std::unique_ptr<sema_detail::TypeChecker> m_typeChecker;
    std::unique_ptr<sema_detail::TypeResolver> m_typeResolver;
    int m_maxClassesPerFile = 0;
    bool m_requireMethodUppercaseStart = false;
    bool m_enforceStrictFileNamingRules = false;
    std::string m_sourceFileStem;
    int m_maxLinesPerMethod = 0;
    int m_maxLinesPerBlockStatement = 0;
    int m_minMethodNameLength = 0;
    bool m_requireClassExplicitVisibility = false;
    bool m_requirePropertyExplicitVisibility = false;
    bool m_requireConstUppercase = false;
    int m_maxNestingDepth = 0;
    bool m_requirePublicMethodDocs = false;
};

} // namespace neuron
