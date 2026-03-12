#pragma once

#include "DocumentManager.h"
#include "lsp/LspDispatcher.h"
#include "lsp/LspTransport.h"

namespace neuron::lsp {

class DefinitionHandler {
public:
  DefinitionHandler(LspDispatcher &dispatcher, DocumentManager &documents,
                    LspTransport &transport);

private:
  DocumentManager &m_documents;
  LspTransport &m_transport;

  DispatchResult handleDefinition(const LspMessageContext &context);
  DispatchResult handleReferences(const LspMessageContext &context);
};

} // namespace neuron::lsp
