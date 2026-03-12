#pragma once

#include "DocumentManager.h"
#include "lsp/LspDispatcher.h"
#include "lsp/LspTransport.h"

namespace neuron::lsp {

class WorkspaceSymbolsHandler {
public:
  WorkspaceSymbolsHandler(LspDispatcher &dispatcher, DocumentManager &documents,
                          LspTransport &transport);

private:
  DocumentManager &m_documents;
  LspTransport &m_transport;

  DispatchResult handleWorkspaceSymbols(const LspMessageContext &context);
};

} // namespace neuron::lsp
