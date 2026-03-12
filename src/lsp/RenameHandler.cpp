#include "RenameHandler.h"

#include "lsp/LspHoverSupport.h"
#include "lsp/LspPath.h"

#include <algorithm>
#include <unordered_map>

namespace neuron::lsp {

RenameHandler::RenameHandler(LspDispatcher &dispatcher, DocumentManager &documents,
                             LspTransport &transport)
    : m_documents(documents), m_transport(transport) {
  dispatcher.registerHandler("textDocument/rename",
                             [this](const LspMessageContext &context) {
                               return handleRename(context);
                             });
}

DispatchResult RenameHandler::handleRename(const LspMessageContext &context) {
  if (context.id == nullptr) {
    return DispatchResult::Continue;
  }
  if (context.params == nullptr) {
    m_transport.sendResult(*context.id, llvm::json::Value(nullptr));
    return DispatchResult::Continue;
  }

  const auto *textDocument = context.params->getObject("textDocument");
  const auto *position = context.params->getObject("position");
  const std::optional<llvm::StringRef> newName = context.params->getString("newName");
  if (textDocument == nullptr || position == nullptr || !newName.has_value()) {
    m_transport.sendResult(*context.id, llvm::json::Value(nullptr));
    return DispatchResult::Continue;
  }
  if (!detail::isValidIdentifier(newName->str())) {
    m_transport.sendError(*context.id, -32602, "Invalid rename target");
    return DispatchResult::Continue;
  }

  const std::optional<llvm::StringRef> uri = textDocument->getString("uri");
  DocumentState *document =
      uri.has_value() ? m_documents.find(std::string(uri->str())) : nullptr;
  if (document == nullptr || document->analyzer == nullptr) {
    m_transport.sendResult(*context.id, llvm::json::Value(nullptr));
    return DispatchResult::Continue;
  }

  const int line = static_cast<int>(position->getInteger("line").value_or(0));
  const int character =
      static_cast<int>(position->getInteger("character").value_or(0));
  const TokenMatch tokenMatch = detail::findTokenAt(*document, line, character);
  if (tokenMatch.token == nullptr || tokenMatch.token->type != TokenType::Identifier) {
    m_transport.sendResult(*context.id, llvm::json::Value(nullptr));
    return DispatchResult::Continue;
  }

  const std::optional<SymbolLocation> definition =
      document->analyzer->getDefinitionLocation(tokenMatch.token->location);
  if (!definition.has_value()) {
    m_transport.sendResult(*context.id, llvm::json::Value(nullptr));
    return DispatchResult::Continue;
  }

  const std::string oldName = tokenMatch.token->value;
  if (oldName == newName->str()) {
    m_transport.sendResult(*context.id,
                           llvm::json::Object{{"changes", llvm::json::Object{}}});
    return DispatchResult::Continue;
  }

  const fs::path workspaceRoot = m_documents.workspaceRootFor(document->path);
  std::unordered_map<std::string, llvm::json::Array> changesByUri;
  for (const fs::path &file : m_documents.collectWorkspaceFiles(workspaceRoot)) {
    const std::optional<std::string> sourceText = m_documents.readWorkspaceFile(file);
    if (!sourceText.has_value() || sourceText->find(oldName) == std::string::npos) {
      continue;
    }

    const std::optional<WorkspaceSemanticState> state =
        m_documents.analyzeWorkspaceFile(file, workspaceRoot);
    if (!state.has_value() || state->semanticResult.analyzer == nullptr) {
      continue;
    }

    for (const Token &token : state->entryTokens) {
      if (token.type != TokenType::Identifier || token.value != oldName) {
        continue;
      }
      const std::optional<SymbolLocation> tokenDefinition =
          state->semanticResult.analyzer->getDefinitionLocation(token.location);
      if (!tokenDefinition.has_value() ||
          !detail::sameSymbolLocation(*tokenDefinition, *definition)) {
        continue;
      }

      const std::string editUri = detail::pathToUri(file);
      changesByUri[editUri].push_back(llvm::json::Object{
          {"range",
           llvm::json::Object{
               {"start",
                llvm::json::Object{
                    {"line", std::max(0, token.location.line - 1)},
                    {"character", std::max(0, token.location.column - 1)}}},
               {"end",
                llvm::json::Object{
                    {"line", std::max(0, token.location.line - 1)},
                    {"character", std::max(0, token.location.column - 1) +
                                      std::max(1, static_cast<int>(oldName.size()))}}}}},
          {"newText", newName->str()}});
    }
  }

  llvm::json::Object changes;
  for (auto &entry : changesByUri) {
    if (!entry.second.empty()) {
      changes[entry.first] = std::move(entry.second);
    }
  }
  llvm::json::Object workspaceEdit;
  workspaceEdit["changes"] = std::move(changes);
  m_transport.sendResult(*context.id, std::move(workspaceEdit));
  return DispatchResult::Continue;
}

} // namespace neuron::lsp
