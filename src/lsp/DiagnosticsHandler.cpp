#include "DiagnosticsHandler.h"

#include "lsp/LspFeaturePayloads.h"
#include "lsp/LspProtocol.h"

namespace neuron::lsp {

namespace {

std::vector<DocumentChange> parseChanges(const llvm::json::Array *changes) {
  std::vector<DocumentChange> parsedChanges;
  if (changes == nullptr) {
    return parsedChanges;
  }

  for (const auto &changeValue : *changes) {
    const auto *change = changeValue.getAsObject();
    if (change == nullptr) {
      continue;
    }

    const std::optional<llvm::StringRef> text = change->getString("text");
    if (!text.has_value()) {
      continue;
    }

    DocumentChange parsed;
    parsed.text = text->str();
    if (const auto *range = change->getObject("range")) {
      parsed.range = detail::parseLspRange(range);
    }
    parsedChanges.push_back(std::move(parsed));
  }

  return parsedChanges;
}

} // namespace

DiagnosticsHandler::DiagnosticsHandler(LspDispatcher &dispatcher,
                                       DocumentManager &documents,
                                       LspTransport &transport)
    : m_documents(documents), m_transport(transport) {
  dispatcher.registerHandler("textDocument/didOpen",
                             [this](const LspMessageContext &context) {
                               return handleDidOpen(context);
                             });
  dispatcher.registerHandler("textDocument/didChange",
                             [this](const LspMessageContext &context) {
                               return handleDidChange(context);
                             });
  dispatcher.registerHandler("textDocument/didClose",
                             [this](const LspMessageContext &context) {
                               return handleDidClose(context);
                             });
}

DispatchResult DiagnosticsHandler::handleDidOpen(const LspMessageContext &context) {
  if (context.params == nullptr) {
    return DispatchResult::Continue;
  }

  const auto *textDocument = context.params->getObject("textDocument");
  if (textDocument == nullptr) {
    return DispatchResult::Continue;
  }

  const std::optional<llvm::StringRef> uri = textDocument->getString("uri");
  if (!uri.has_value()) {
    return DispatchResult::Continue;
  }

  std::string text;
  if (const std::optional<llvm::StringRef> textValue =
          textDocument->getString("text")) {
    text = textValue->str();
  }
  const int version =
      static_cast<int>(textDocument->getInteger("version").value_or(0));

  DocumentState &document =
      m_documents.openDocument(std::string(uri->str()), std::move(text), version);
  m_documents.reanalyze(document);
  publish(document.uri, document.diagnostics);
  return DispatchResult::Continue;
}

DispatchResult DiagnosticsHandler::handleDidChange(
    const LspMessageContext &context) {
  if (context.params == nullptr) {
    return DispatchResult::Continue;
  }

  const auto *textDocument = context.params->getObject("textDocument");
  const auto *changes = context.params->getArray("contentChanges");
  if (textDocument == nullptr || changes == nullptr) {
    return DispatchResult::Continue;
  }

  const std::optional<llvm::StringRef> uri = textDocument->getString("uri");
  if (!uri.has_value()) {
    return DispatchResult::Continue;
  }

  DocumentState *existing = m_documents.find(std::string(uri->str()));
  const int version = static_cast<int>(
      textDocument->getInteger("version")
          .value_or(existing != nullptr ? existing->version : 0));
  DocumentState *document =
      m_documents.applyChanges(std::string(uri->str()), parseChanges(changes),
                               version);
  if (document == nullptr) {
    return DispatchResult::Continue;
  }

  m_documents.reanalyze(*document);
  publish(document->uri, document->diagnostics);
  return DispatchResult::Continue;
}

DispatchResult DiagnosticsHandler::handleDidClose(
    const LspMessageContext &context) {
  if (context.params == nullptr) {
    return DispatchResult::Continue;
  }

  const auto *textDocument = context.params->getObject("textDocument");
  if (textDocument == nullptr) {
    return DispatchResult::Continue;
  }

  const std::optional<llvm::StringRef> uri = textDocument->getString("uri");
  if (!uri.has_value()) {
    return DispatchResult::Continue;
  }

  publish(std::string(uri->str()), {});
  m_documents.closeDocument(std::string(uri->str()));
  return DispatchResult::Continue;
}

void DiagnosticsHandler::publish(
    const std::string &uri,
    const std::vector<frontend::Diagnostic> &diagnostics) {
  llvm::json::Array diagnosticArray;
  for (const auto &diagnostic : diagnostics) {
    diagnosticArray.push_back(detail::makeLspDiagnostic(diagnostic));
  }

  llvm::json::Object params;
  params["uri"] = uri;
  params["diagnostics"] = std::move(diagnosticArray);
  m_transport.sendNotification("textDocument/publishDiagnostics",
                               std::move(params));
}

} // namespace neuron::lsp
