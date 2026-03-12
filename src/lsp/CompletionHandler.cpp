#include "CompletionHandler.h"

#include "lsp/LspCompletionSupport.h"
#include "lsp/LspFeaturePayloads.h"
#include "lsp/LspText.h"

namespace neuron::lsp {

CompletionHandler::CompletionHandler(LspDispatcher &dispatcher,
                                     DocumentManager &documents,
                                     LspTransport &transport)
    : m_documents(documents), m_transport(transport) {
  dispatcher.registerHandler("textDocument/completion",
                             [this](const LspMessageContext &context) {
                               return handleCompletion(context);
                             });
  dispatcher.registerHandler("textDocument/signatureHelp",
                             [this](const LspMessageContext &context) {
                               return handleSignatureHelp(context);
                             });
}

DispatchResult CompletionHandler::handleCompletion(
    const LspMessageContext &context) {
  if (context.id == nullptr) {
    return DispatchResult::Continue;
  }

  llvm::json::Object result;
  result["isIncomplete"] = false;
  result["items"] = llvm::json::Array{};
  if (context.params == nullptr) {
    m_transport.sendResult(*context.id, std::move(result));
    return DispatchResult::Continue;
  }

  const auto *textDocument = context.params->getObject("textDocument");
  const auto *position = context.params->getObject("position");
  if (textDocument == nullptr || position == nullptr) {
    m_transport.sendResult(*context.id, std::move(result));
    return DispatchResult::Continue;
  }

  const std::optional<llvm::StringRef> uri = textDocument->getString("uri");
  const DocumentState *document =
      uri.has_value() ? m_documents.find(std::string(uri->str())) : nullptr;
  if (document == nullptr || document->analyzer == nullptr) {
    m_transport.sendResult(*context.id, std::move(result));
    return DispatchResult::Continue;
  }

  const int line = static_cast<int>(position->getInteger("line").value_or(0));
  const int character =
      static_cast<int>(position->getInteger("character").value_or(0));
  llvm::json::Array items;
  for (const auto &symbol :
       detail::collectCompletionSymbols(*document, line, character)) {
    items.push_back(detail::makeLspCompletionItem(symbol, document->analyzer.get()));
  }
  result["items"] = std::move(items);
  m_transport.sendResult(*context.id, std::move(result));
  return DispatchResult::Continue;
}

DispatchResult CompletionHandler::handleSignatureHelp(
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
  const std::size_t cursorOffset =
      detail::positionToOffset(document->text, line, character);
  const std::optional<detail::ActiveCallContext> activeCall =
      detail::findActiveCallContext(document->text, cursorOffset);
  if (!activeCall.has_value()) {
    m_transport.sendResult(*context.id, llvm::json::Value(nullptr));
    return DispatchResult::Continue;
  }

  const SourceLocation location = {line + 1, character + 1,
                                   document->path.string()};
  const std::vector<VisibleSymbolInfo> visibleSymbols =
      document->analyzer->getScopeSnapshot(location);
  const std::vector<CallableSignatureInfo> signatures =
      detail::resolveCallableSignatures(document->analyzer.get(), visibleSymbols,
                                        activeCall->callableChain);
  if (signatures.empty()) {
    m_transport.sendResult(*context.id, llvm::json::Value(nullptr));
    return DispatchResult::Continue;
  }

  m_transport.sendResult(
      *context.id,
      detail::makeLspSignatureHelp(signatures, activeCall->activeParameter));
  return DispatchResult::Continue;
}

} // namespace neuron::lsp
