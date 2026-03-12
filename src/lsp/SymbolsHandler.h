#pragma once

#include "DocumentManager.h"
#include "lsp/LspDispatcher.h"
#include "lsp/LspTransport.h"

namespace neuron::lsp {

class SymbolsHandler {
public:
  SymbolsHandler(LspDispatcher &dispatcher, DocumentManager &documents,
                 LspTransport &transport);

private:
  DocumentManager &m_documents;
  LspTransport &m_transport;

  DispatchResult handleInlayHints(const LspMessageContext &context);
  DispatchResult handleSemanticTokens(const LspMessageContext &context);
  DispatchResult handleDocumentSymbols(const LspMessageContext &context);
};

} // namespace neuron::lsp
