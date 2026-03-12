#pragma once

#include "LspTypes.h"

namespace neuron::lsp::detail {

const BindingDeclNode *findBindingInDocument(const DocumentState &document,
                                             const SourceLocation &location);
EnclosingMethodContext findEnclosingMethodContext(const DocumentState &document,
                                                  const SourceLocation &location);
const CallExprNode *findCallInDocument(const DocumentState &document,
                                       const SourceLocation &location,
                                       EnclosingMethodContext *outContext);
frontend::Diagnostic makeUnusedVariableDiagnostic(const BindingDeclNode *binding);
std::vector<frontend::Diagnostic>
collectUnusedVariableDiagnostics(const DocumentState &document);

} // namespace neuron::lsp::detail
