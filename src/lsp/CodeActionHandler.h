#pragma once

#include "DocumentManager.h"
#include "lsp/LspDispatcher.h"
#include "lsp/LspTransport.h"

namespace neuron::lsp {

class CodeActionHandler {
public:
  CodeActionHandler(LspDispatcher &dispatcher, DocumentManager &documents,
                    LspTransport &transport);

private:
  DocumentManager &m_documents;
  LspTransport &m_transport;

  DispatchResult handleCodeAction(const LspMessageContext &context);
};

} // namespace neuron::lsp
