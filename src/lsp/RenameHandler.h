#pragma once

#include "DocumentManager.h"
#include "lsp/LspDispatcher.h"
#include "lsp/LspTransport.h"

namespace neuron::lsp {

class RenameHandler {
public:
  RenameHandler(LspDispatcher &dispatcher, DocumentManager &documents,
                LspTransport &transport);

private:
  DocumentManager &m_documents;
  LspTransport &m_transport;

  DispatchResult handleRename(const LspMessageContext &context);
};

} // namespace neuron::lsp
