#pragma once

#include "LspTypes.h"

#include <string_view>

namespace neuron::lsp::detail {

struct DebugViewContent {
  std::string title;
  std::string languageId;
  std::string content;
};

void refreshMacroExpansionState(DocumentState &document, const fs::path &toolRoot);
std::string renderTokensAsSource(const std::vector<Token> &tokens);
DebugViewContent buildDebugView(const DocumentState &document,
                                const fs::path &workspaceRoot,
                                const fs::path &toolRoot,
                                std::string_view viewKind);

} // namespace neuron::lsp::detail
