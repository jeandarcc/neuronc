#pragma once

#include "neuronc/sema/SemanticAnalyzer.h"

namespace neuron::sema_detail {

class ReferenceTracker {
public:
  void reset();

  NTypePtr rememberType(const ASTNode *node, NTypePtr type);
  NTypePtr inferredType(const ASTNode *node) const;

  void setDocumentDeclarations(const std::vector<ASTNode *> &declarations);
  const std::vector<ASTNode *> &documentDeclarations() const;

  void recordDefinition(Symbol *symbol, const SourceLocation &loc, int length);
  void recordReference(Symbol *symbol, const SourceLocation &loc, int length);

  std::optional<SymbolLocation>
  definitionLocation(const SourceLocation &referenceLocation) const;
  std::vector<SymbolLocation>
  referenceLocations(const SourceLocation &referenceLocation) const;
  std::vector<DocumentSymbolInfo> documentSymbols() const;
  std::optional<VisibleSymbolInfo>
  resolvedSymbol(const SourceLocation &location) const;

private:
  static std::string makeLocationKey(const SourceLocation &loc);
  static SymbolRange makePointRange(const SourceLocation &loc, int length);
  const Symbol *findSymbol(const SourceLocation &loc) const;
  std::optional<DocumentSymbolInfo>
  buildDocumentSymbol(const ASTNode *node, bool classMember = false) const;

  std::unordered_map<const ASTNode *, NTypePtr> m_inferredTypes;
  std::vector<ASTNode *> m_documentDeclarations;
  std::unordered_map<std::string, Symbol *> m_symbolIndex;
};

} // namespace neuron::sema_detail
