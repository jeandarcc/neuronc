#include "HoverHandler.h"

#include "lsp/LspHoverSupport.h"

namespace neuron::lsp {

HoverHandler::HoverHandler(LspDispatcher &dispatcher, DocumentManager &documents,
                           LspTransport &transport)
    : m_documents(documents), m_transport(transport) {
  dispatcher.registerHandler("textDocument/hover",
                             [this](const LspMessageContext &context) {
                               return handleHover(context);
                             });
}

DispatchResult HoverHandler::handleHover(const LspMessageContext &context) {
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
  if (document == nullptr) {
    m_transport.sendResult(*context.id, llvm::json::Value(nullptr));
    return DispatchResult::Continue;
  }

  const int line = static_cast<int>(position->getInteger("line").value_or(0));
  const int character =
      static_cast<int>(position->getInteger("character").value_or(0));
  const TokenMatch tokenMatch = detail::findTokenAt(*document, line, character);
  if (tokenMatch.token == nullptr) {
    m_transport.sendResult(*context.id, llvm::json::Value(nullptr));
    return DispatchResult::Continue;
  }

  const ASTNode *node =
      detail::findBestNodeAt(*document, tokenMatch.token->location.line,
                             tokenMatch.token->location.column, tokenMatch);
  const std::optional<std::string> markup =
      detail::buildHoverMarkup(node, document->analyzer.get());
  if (!markup.has_value()) {
    m_transport.sendResult(*context.id, llvm::json::Value(nullptr));
    return DispatchResult::Continue;
  }

  llvm::json::Object hover;
  hover["contents"] =
      llvm::json::Object{{"kind", "markdown"}, {"value", *markup}};
  hover["range"] = llvm::json::Object{
      {"start",
       llvm::json::Object{{"line", tokenMatch.token->location.line - 1},
                          {"character", tokenMatch.token->location.column - 1}}},
      {"end",
       llvm::json::Object{
           {"line", tokenMatch.token->location.line - 1},
           {"character", tokenMatch.token->location.column - 1 +
                             std::max(1, static_cast<int>(
                                              tokenMatch.token->value.size()))}}}};
  m_transport.sendResult(*context.id, std::move(hover));
  return DispatchResult::Continue;
}

} // namespace neuron::lsp
