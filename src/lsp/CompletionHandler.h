#pragma once

#include "DocumentManager.h"
#include "lsp/LspDispatcher.h"
#include "lsp/LspTransport.h"

namespace neuron::lsp {

class CompletionHandler {
public:
  CompletionHandler(LspDispatcher &dispatcher, DocumentManager &documents,
                    LspTransport &transport);

private:
  DocumentManager &m_documents;
  LspTransport &m_transport;

  DispatchResult handleCompletion(const LspMessageContext &context);
  DispatchResult handleSignatureHelp(const LspMessageContext &context);
};

} // namespace neuron::lsp
