#pragma once

#include "DocumentManager.h"
#include "lsp/LspDispatcher.h"
#include "lsp/LspTransport.h"

namespace neuron::lsp {

class DiagnosticsHandler {
public:
  DiagnosticsHandler(LspDispatcher &dispatcher, DocumentManager &documents,
                     LspTransport &transport);

private:
  DocumentManager &m_documents;
  LspTransport &m_transport;

  DispatchResult handleDidOpen(const LspMessageContext &context);
  DispatchResult handleDidChange(const LspMessageContext &context);
  DispatchResult handleDidClose(const LspMessageContext &context);
  void publish(const std::string &uri,
               const std::vector<frontend::Diagnostic> &diagnostics);
};

} // namespace neuron::lsp
