#pragma once

#include "LspTypes.h"

namespace neuron::lsp::detail {

struct ActiveCallContext {
  std::vector<std::string> callableChain;
  std::size_t activeParameter = 0;
};

std::vector<VisibleSymbolInfo> collectCompletionSymbols(const DocumentState &document,
                                                        int line, int character);
std::vector<CallableSignatureInfo>
resolveCallableSignatures(const SemanticAnalyzer *analyzer,
                          const std::vector<VisibleSymbolInfo> &visibleSymbols,
                          const std::vector<std::string> &callableChain);
std::optional<ActiveCallContext> findActiveCallContext(const std::string &text,
                                                       std::size_t cursorOffset);

} // namespace neuron::lsp::detail
