#pragma once

#include "LspTypes.h"

namespace neuron::lsp::detail {

std::string typeToString(const NTypePtr &type);
std::string formatMethodSignature(const MethodDeclNode *method);
const ASTNode *findBestNodeAt(const DocumentState &document, int line, int column,
                              const TokenMatch &match);
TokenMatch findTokenAt(const DocumentState &document, int line, int character);
std::optional<std::string> buildHoverMarkup(const ASTNode *node,
                                            const SemanticAnalyzer *analyzer);
std::optional<VisibleSymbolInfo>
resolveSymbolAtPosition(const DocumentState &document, int line, int character,
                        SourceLocation *outLocation, const ASTNode **outNode);

} // namespace neuron::lsp::detail
