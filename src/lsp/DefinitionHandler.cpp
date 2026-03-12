#include "DefinitionHandler.h"

#include "lsp/LspHoverSupport.h"
#include "lsp/LspProtocol.h"

#include <algorithm>

namespace neuron::lsp {

DefinitionHandler::DefinitionHandler(LspDispatcher &dispatcher,
                                     DocumentManager &documents,
                                     LspTransport &transport)
    : m_documents(documents), m_transport(transport) {
  dispatcher.registerHandler("textDocument/definition",
                             [this](const LspMessageContext &context) {
                               return handleDefinition(context);
                             });
  dispatcher.registerHandler("textDocument/references",
                             [this](const LspMessageContext &context) {
                               return handleReferences(context);
                             });
}

DispatchResult DefinitionHandler::handleDefinition(
    const LspMessageContext &context) {
  if (context.id == nullptr) {
    return DispatchResult::Continue;
  }
  if (context.params == nullptr) {
    m_transport.sendResult(*context.id, llvm::json::Value(nullptr));
    return DispatchResult::Continue;
  }

  const auto *textDocument = context.params->getObject("textDocument");
  const auto *position = context.params->getObject("position");
  if (textDocument == nullptr || position == nullptr) {
    m_transport.sendResult(*context.id, llvm::json::Value(nullptr));
    return DispatchResult::Continue;
  }

  const std::optional<llvm::StringRef> uri = textDocument->getString("uri");
  const DocumentState *document =
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

  m_transport.sendResult(*context.id,
                         llvm::json::Value(detail::makeLspLocation(*definition)));
  return DispatchResult::Continue;
}

DispatchResult DefinitionHandler::handleReferences(
    const LspMessageContext &context) {
  if (context.id == nullptr) {
    return DispatchResult::Continue;
  }
  if (context.params == nullptr) {
    m_transport.sendResult(*context.id, llvm::json::Array{});
    return DispatchResult::Continue;
  }

  const auto *textDocument = context.params->getObject("textDocument");
  const auto *position = context.params->getObject("position");
  const auto *requestContext = context.params->getObject("context");
  if (textDocument == nullptr || position == nullptr) {
    m_transport.sendResult(*context.id, llvm::json::Array{});
    return DispatchResult::Continue;
  }

  const std::optional<llvm::StringRef> uri = textDocument->getString("uri");
  const DocumentState *document =
      uri.has_value() ? m_documents.find(std::string(uri->str())) : nullptr;
  if (document == nullptr || document->analyzer == nullptr) {
    m_transport.sendResult(*context.id, llvm::json::Array{});
    return DispatchResult::Continue;
  }

  const int line = static_cast<int>(position->getInteger("line").value_or(0));
  const int character =
      static_cast<int>(position->getInteger("character").value_or(0));
  const TokenMatch tokenMatch = detail::findTokenAt(*document, line, character);
  if (tokenMatch.token == nullptr || tokenMatch.token->type != TokenType::Identifier) {
    m_transport.sendResult(*context.id, llvm::json::Array{});
    return DispatchResult::Continue;
  }

  const bool includeDeclaration =
      requestContext != nullptr &&
      requestContext->getBoolean("includeDeclaration").value_or(false);
  llvm::json::Array references;
  std::vector<SymbolLocation> uniqueLocations;
  const auto appendUnique = [&](const SymbolLocation &location) {
    const auto sameLocation = [&](const SymbolLocation &existing) {
      return existing.location.file == location.location.file &&
             existing.location.line == location.location.line &&
             existing.location.column == location.location.column;
    };
    if (std::find_if(uniqueLocations.begin(), uniqueLocations.end(), sameLocation) !=
        uniqueLocations.end()) {
      return;
    }
    uniqueLocations.push_back(location);
    references.push_back(detail::makeLspLocation(location));
  };

  if (includeDeclaration) {
    const std::optional<SymbolLocation> definition =
        document->analyzer->getDefinitionLocation(tokenMatch.token->location);
    if (definition.has_value()) {
      appendUnique(*definition);
    }
  }

  for (const auto &reference :
       document->analyzer->getReferenceLocations(tokenMatch.token->location)) {
    appendUnique(reference);
  }

  m_transport.sendResult(*context.id, std::move(references));
  return DispatchResult::Continue;
}

} // namespace neuron::lsp
