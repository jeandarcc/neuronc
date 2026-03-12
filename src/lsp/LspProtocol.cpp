#include "LspProtocol.h"

#include "LspPath.h"

#include <algorithm>

namespace neuron::lsp::detail {

llvm::json::Object makeLspPosition(const frontend::Position &position) {
  llvm::json::Object object;
  object["line"] = std::max(0, position.line - 1);
  object["character"] = std::max(0, position.column - 1);
  return object;
}

llvm::json::Object makeLspPosition(const SourceLocation &location) {
  llvm::json::Object object;
  object["line"] = std::max(0, location.line - 1);
  object["character"] = std::max(0, location.column - 1);
  return object;
}

llvm::json::Object makeLspRange(const frontend::Range &range) {
  llvm::json::Object object;
  object["start"] = makeLspPosition(range.start);
  object["end"] = makeLspPosition(range.end);
  return object;
}

llvm::json::Object makeLspRange(const SymbolRange &range) {
  llvm::json::Object object;
  object["start"] = makeLspPosition(range.start);
  object["end"] = makeLspPosition(range.end);
  return object;
}

llvm::json::Object makeLspLocation(const SymbolLocation &location) {
  llvm::json::Object object;
  object["uri"] = pathToUri(fs::path(location.location.file));
  object["range"] = llvm::json::Object{
      {"start", makeLspPosition(location.location)},
      {"end",
       llvm::json::Object{
           {"line", std::max(0, location.location.line - 1)},
           {"character", std::max(0, location.location.column - 1) +
                             std::max(1, location.length)}}}};
  return object;
}

frontend::Range makeFrontendRange(int startLine, int startColumn, int endLine,
                                  int endColumn) {
  frontend::Range range;
  range.start = {std::max(1, startLine), std::max(1, startColumn)};
  range.end = {std::max(1, endLine), std::max(1, endColumn)};
  return range;
}

llvm::json::Object makeLspTextEdit(const frontend::Range &range,
                                   std::string newText) {
  llvm::json::Object object;
  object["range"] = makeLspRange(range);
  object["newText"] = std::move(newText);
  return object;
}

llvm::json::Object makeWorkspaceEditForUri(const std::string &uri,
                                           llvm::json::Array edits) {
  llvm::json::Object changes;
  changes[uri] = std::move(edits);

  llvm::json::Object edit;
  edit["changes"] = std::move(changes);
  return edit;
}

frontend::Range parseLspRange(const llvm::json::Object *rangeObject) {
  frontend::Range range;
  if (rangeObject == nullptr) {
    return range;
  }

  const auto *start = rangeObject->getObject("start");
  const auto *end = rangeObject->getObject("end");
  if (start != nullptr) {
    range.start.line = static_cast<int>(start->getInteger("line").value_or(0)) + 1;
    range.start.column =
        static_cast<int>(start->getInteger("character").value_or(0)) + 1;
  }
  if (end != nullptr) {
    range.end.line = static_cast<int>(end->getInteger("line").value_or(0)) + 1;
    range.end.column =
        static_cast<int>(end->getInteger("character").value_or(0)) + 1;
  }
  return range;
}

int comparePositions(const frontend::Position &lhs, const frontend::Position &rhs) {
  if (lhs.line != rhs.line) {
    return lhs.line < rhs.line ? -1 : 1;
  }
  if (lhs.column != rhs.column) {
    return lhs.column < rhs.column ? -1 : 1;
  }
  return 0;
}

bool positionWithinRange(const frontend::Position &position,
                         const frontend::Range &range) {
  return comparePositions(position, range.start) >= 0 &&
         comparePositions(position, range.end) <= 0;
}

bool rangeOverlaps(const frontend::Range &lhs, const frontend::Range &rhs) {
  return comparePositions(lhs.end, rhs.start) >= 0 &&
         comparePositions(rhs.end, lhs.start) >= 0;
}

} // namespace neuron::lsp::detail
