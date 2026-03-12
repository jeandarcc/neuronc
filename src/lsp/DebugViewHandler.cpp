#include "DebugViewHandler.h"

#include "lsp/LspDebugViews.h"
#include "lsp/LspPath.h"
#include "lsp/LspProtocol.h"

namespace neuron::lsp {

namespace {

llvm::json::Object makeDebugViewCommand(std::string title, const std::string &uri,
                                        std::string viewKind) {
  llvm::json::Array arguments;
  arguments.push_back(uri);
  arguments.push_back(std::move(viewKind));
  return llvm::json::Object{{"title", std::move(title)},
                            {"command", "neuron.openDebugView"},
                            {"arguments", std::move(arguments)}};
}

llvm::json::Object makeCodeLensAtDocumentTop(const std::string &uri,
                                             std::string title,
                                             std::string viewKind) {
  return llvm::json::Object{
      {"range", detail::makeLspRange(detail::makeFrontendRange(1, 1, 1, 1))},
      {"command",
       makeDebugViewCommand(std::move(title), uri, std::move(viewKind))}};
}

DocumentState makeEphemeralDocument(const std::string &uri, fs::path path,
                                    std::string text) {
  DocumentState document;
  document.uri = uri;
  document.path = std::move(path);
  document.text = std::move(text);
  return document;
}

} // namespace

DebugViewHandler::DebugViewHandler(LspDispatcher &dispatcher,
                                   DocumentManager &documents,
                                   LspTransport &transport)
    : m_documents(documents), m_transport(transport) {
  dispatcher.registerHandler("textDocument/codeLens",
                             [this](const LspMessageContext &context) {
                               return handleCodeLens(context);
                             });
  dispatcher.registerHandler("neuron/textDocument/debugView",
                             [this](const LspMessageContext &context) {
                               return handleDebugView(context);
                             });
}

DispatchResult DebugViewHandler::handleCodeLens(
    const LspMessageContext &context) {
  if (context.id == nullptr) {
    return DispatchResult::Continue;
  }
  if (context.params == nullptr) {
    m_transport.sendResult(*context.id, llvm::json::Array{});
    return DispatchResult::Continue;
  }

  const auto *textDocument = context.params->getObject("textDocument");
  const std::optional<llvm::StringRef> uri =
      textDocument != nullptr ? textDocument->getString("uri") : std::nullopt;
  if (!uri.has_value()) {
    m_transport.sendResult(*context.id, llvm::json::Array{});
    return DispatchResult::Continue;
  }

  llvm::json::Array codeLenses;
  codeLenses.push_back(
      makeCodeLensAtDocumentTop(std::string(uri->str()), "Show Expanded Source",
                                "expanded"));
  codeLenses.push_back(
      makeCodeLensAtDocumentTop(std::string(uri->str()), "Show NIR", "nir"));
  codeLenses.push_back(
      makeCodeLensAtDocumentTop(std::string(uri->str()), "Show MIR", "mir"));
  m_transport.sendResult(*context.id, std::move(codeLenses));
  return DispatchResult::Continue;
}

DispatchResult DebugViewHandler::handleDebugView(
    const LspMessageContext &context) {
  if (context.id == nullptr) {
    return DispatchResult::Continue;
  }
  if (context.params == nullptr) {
    m_transport.sendResult(*context.id, llvm::json::Value(nullptr));
    return DispatchResult::Continue;
  }

  const auto *textDocument = context.params->getObject("textDocument");
  const std::optional<llvm::StringRef> uri =
      textDocument != nullptr ? textDocument->getString("uri") : std::nullopt;
  const std::optional<llvm::StringRef> viewKind =
      context.params->getString("view");
  if (!uri.has_value() || !viewKind.has_value()) {
    m_transport.sendResult(*context.id, llvm::json::Value(nullptr));
    return DispatchResult::Continue;
  }

  const fs::path path = detail::uriToPath(uri->str());
  const fs::path workspaceRoot = m_documents.workspaceRootFor(path);
  const DocumentState *openDocument = m_documents.find(std::string(uri->str()));
  DocumentState ephemeralDocument;
  const DocumentState *document = openDocument;
  if (document == nullptr) {
    const std::optional<std::string> text = m_documents.readWorkspaceFile(path);
    if (!text.has_value()) {
      m_transport.sendResult(*context.id, llvm::json::Value(nullptr));
      return DispatchResult::Continue;
    }
    ephemeralDocument =
        makeEphemeralDocument(std::string(uri->str()), path, *text);
    detail::refreshMacroExpansionState(ephemeralDocument, m_documents.toolRoot());
    document = &ephemeralDocument;
  }

  const detail::DebugViewContent view =
      detail::buildDebugView(*document, workspaceRoot, m_documents.toolRoot(),
                             viewKind->str());
  llvm::json::Object result;
  result["title"] = view.title;
  result["languageId"] = view.languageId;
  result["content"] = view.content;
  m_transport.sendResult(*context.id, std::move(result));
  return DispatchResult::Continue;
}

} // namespace neuron::lsp
