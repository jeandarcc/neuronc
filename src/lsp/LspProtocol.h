#pragma once

#include "LspTypes.h"

namespace neuron::lsp::detail {

llvm::json::Object makeLspPosition(const frontend::Position &position);
llvm::json::Object makeLspPosition(const SourceLocation &location);
llvm::json::Object makeLspRange(const frontend::Range &range);
llvm::json::Object makeLspRange(const SymbolRange &range);
llvm::json::Object makeLspLocation(const SymbolLocation &location);
frontend::Range makeFrontendRange(int startLine, int startColumn, int endLine,
                                  int endColumn);
llvm::json::Object makeLspTextEdit(const frontend::Range &range,
                                   std::string newText);
llvm::json::Object makeWorkspaceEditForUri(const std::string &uri,
                                           llvm::json::Array edits);
frontend::Range parseLspRange(const llvm::json::Object *rangeObject);
int comparePositions(const frontend::Position &lhs, const frontend::Position &rhs);
bool positionWithinRange(const frontend::Position &position,
                         const frontend::Range &range);
bool rangeOverlaps(const frontend::Range &lhs, const frontend::Range &rhs);

} // namespace neuron::lsp::detail
