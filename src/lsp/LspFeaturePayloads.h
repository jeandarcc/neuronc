#pragma once

#include "LspTypes.h"

namespace neuron::lsp::detail {

std::optional<int> toSemanticTokenType(SymbolKind kind);
void appendSemanticToken(std::vector<SemanticTokenEntry> *tokens, int line,
                         int character, int length, int tokenType);
llvm::json::Value encodeSemanticTokens(
    std::vector<SemanticTokenEntry> semanticTokens);
int toLspSymbolKind(SymbolKind kind);
llvm::json::Value makeLspDocumentSymbol(const DocumentSymbolInfo &symbol);
llvm::json::Value makeLspWorkspaceSymbol(const WorkspaceSymbolEntry &symbol);
llvm::json::Value makeLspDiagnostic(const frontend::Diagnostic &diagnostic);
llvm::json::Object makeHierarchyItemData(const SymbolLocation &location,
                                         std::string_view name);
std::optional<SymbolLocation>
parseHierarchyItemLocation(const llvm::json::Object *itemObject);
llvm::json::Object makeCallHierarchyItem(const VisibleSymbolInfo &symbol,
                                         const SymbolLocation &definition,
                                         const ASTNode *root,
                                         const SemanticAnalyzer *analyzer);
llvm::json::Object makeTypeHierarchyItem(const WorkspaceTypeRecord &record);
llvm::json::Value makeLspCompletionItem(const VisibleSymbolInfo &symbol,
                                        const SemanticAnalyzer *analyzer);
llvm::json::Value makeLspSignatureHelp(
    const std::vector<CallableSignatureInfo> &signatures,
    std::size_t activeParameter);
llvm::json::Object makeServerCapabilities();

} // namespace neuron::lsp::detail
