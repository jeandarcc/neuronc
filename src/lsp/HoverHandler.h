#pragma once

#include "DocumentManager.h"
#include "lsp/LspDispatcher.h"
#include "lsp/LspTransport.h"

namespace neuron::lsp {

class HoverHandler {
public:
  HoverHandler(LspDispatcher &dispatcher, DocumentManager &documents,
               LspTransport &transport);

private:
  DocumentManager &m_documents;
  LspTransport &m_transport;

  DispatchResult handleHover(const LspMessageContext &context);
};

} // namespace neuron::lsp
