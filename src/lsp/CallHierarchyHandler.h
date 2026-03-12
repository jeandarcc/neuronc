#pragma once

#include "DocumentManager.h"
#include "lsp/LspDispatcher.h"
#include "lsp/LspTransport.h"

namespace neuron::lsp {

class CallHierarchyHandler {
public:
  CallHierarchyHandler(LspDispatcher &dispatcher, DocumentManager &documents,
                       LspTransport &transport);

private:
  DocumentManager &m_documents;
  LspTransport &m_transport;

  DispatchResult handlePrepare(const LspMessageContext &context);
  DispatchResult handleIncoming(const LspMessageContext &context);
  DispatchResult handleOutgoing(const LspMessageContext &context);
};

} // namespace neuron::lsp
