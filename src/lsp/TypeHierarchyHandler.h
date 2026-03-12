#pragma once

#include "DocumentManager.h"
#include "lsp/LspDispatcher.h"
#include "lsp/LspTransport.h"

namespace neuron::lsp {

class TypeHierarchyHandler {
public:
  TypeHierarchyHandler(LspDispatcher &dispatcher, DocumentManager &documents,
                       LspTransport &transport);

private:
  DocumentManager &m_documents;
  LspTransport &m_transport;

  DispatchResult handlePrepare(const LspMessageContext &context);
  DispatchResult handleSupertypes(const LspMessageContext &context);
  DispatchResult handleSubtypes(const LspMessageContext &context);
};

} // namespace neuron::lsp
