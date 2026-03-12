#include "WorkspaceSymbolsHandler.h"

#include "lsp/LspFeaturePayloads.h"
#include "lsp/LspPath.h"
#include "lsp/LspSymbols.h"

#include <algorithm>

namespace neuron::lsp {

WorkspaceSymbolsHandler::WorkspaceSymbolsHandler(LspDispatcher &dispatcher,
                                                 DocumentManager &documents,
                                                 LspTransport &transport)
    : m_documents(documents), m_transport(transport) {
  dispatcher.registerHandler("workspace/symbol",
                             [this](const LspMessageContext &context) {
                               return handleWorkspaceSymbols(context);
                             });
}

DispatchResult WorkspaceSymbolsHandler::handleWorkspaceSymbols(
    const LspMessageContext &context) {
  if (context.id == nullptr) {
    return DispatchResult::Continue;
  }

  std::string query;
  if (context.params != nullptr) {
    if (const std::optional<llvm::StringRef> queryValue =
            context.params->getString("query")) {
      query = queryValue->str();
    }
  }

  const fs::path workspaceRoot = m_documents.effectiveWorkspaceRoot();
  std::vector<WorkspaceSymbolEntry> symbols = detail::collectWorkspaceSymbols(
      m_documents.collectWorkspaceFiles(workspaceRoot),
      [&](const fs::path &file) { return m_documents.readWorkspaceFile(file); });
  const std::string loweredQuery = detail::toLowerCopy(query);

  const auto matchScore = [&](const WorkspaceSymbolEntry &symbol) {
    if (loweredQuery.empty()) {
      return 0;
    }

    const std::string loweredName = detail::toLowerCopy(symbol.name);
    if (loweredName == loweredQuery) {
      return 0;
    }
    if (detail::startsWith(loweredName, loweredQuery)) {
      return 1;
    }
    if (detail::containsCaseInsensitive(symbol.name, loweredQuery) ||
        detail::containsCaseInsensitive(symbol.containerName, loweredQuery)) {
      return 2;
    }
    return 3;
  };

  std::sort(symbols.begin(), symbols.end(),
            [&](const WorkspaceSymbolEntry &lhs,
                const WorkspaceSymbolEntry &rhs) {
              const int lhsScore = matchScore(lhs);
              const int rhsScore = matchScore(rhs);
              if (lhsScore != rhsScore) {
                return lhsScore < rhsScore;
              }
              if (lhs.name != rhs.name) {
                return lhs.name < rhs.name;
              }
              if (lhs.range.start.file != rhs.range.start.file) {
                return lhs.range.start.file < rhs.range.start.file;
              }
              if (lhs.range.start.line != rhs.range.start.line) {
                return lhs.range.start.line < rhs.range.start.line;
              }
              return lhs.range.start.column < rhs.range.start.column;
            });

  llvm::json::Array result;
  for (const WorkspaceSymbolEntry &symbol : symbols) {
    if (!loweredQuery.empty() &&
        !detail::containsCaseInsensitive(symbol.name, loweredQuery) &&
        !detail::containsCaseInsensitive(symbol.containerName, loweredQuery)) {
      continue;
    }
    result.push_back(detail::makeLspWorkspaceSymbol(symbol));
    if (result.size() >= kMaxWorkspaceSymbolResults) {
      break;
    }
  }

  m_transport.sendResult(*context.id, std::move(result));
  return DispatchResult::Continue;
}

} // namespace neuron::lsp
