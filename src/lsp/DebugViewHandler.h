#pragma once

#include "DocumentManager.h"
#include "lsp/LspDispatcher.h"
#include "lsp/LspTransport.h"

namespace neuron::lsp {

class DebugViewHandler {
public:
  DebugViewHandler(LspDispatcher &dispatcher, DocumentManager &documents,
                   LspTransport &transport);

private:
  DocumentManager &m_documents;
  LspTransport &m_transport;

  DispatchResult handleCodeLens(const LspMessageContext &context);
  DispatchResult handleDebugView(const LspMessageContext &context);
};

} // namespace neuron::lsp
