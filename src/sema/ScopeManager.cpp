#include "ScopeManager.h"

namespace neuron::sema_detail {

void ScopeManager::reset() { m_scopeSnapshots.clear(); }

void ScopeManager::recordSnapshot(const SourceLocation &location,
                                  std::vector<VisibleSymbolInfo> symbols) {
  if (location.file.empty()) {
    return;
  }

  ScopeSnapshotEntry entry;
  entry.location = location;
  entry.symbols = std::move(symbols);
  m_scopeSnapshots.push_back(std::move(entry));
}

std::vector<VisibleSymbolInfo>
ScopeManager::snapshotAt(const SourceLocation &location) const {
  const ScopeSnapshotEntry *best = nullptr;
  for (const auto &snapshot : m_scopeSnapshots) {
    if (snapshot.location.file != location.file) {
      continue;
    }
    if (!isLocationAtOrBefore(snapshot.location, location)) {
      continue;
    }
    best = &snapshot;
  }

  if (best != nullptr) {
    return best->symbols;
  }
  return {};
}

bool ScopeManager::isLocationAtOrBefore(const SourceLocation &lhs,
                                        const SourceLocation &rhs) {
  if (lhs.file != rhs.file) {
    return lhs.file < rhs.file;
  }
  if (lhs.line != rhs.line) {
    return lhs.line < rhs.line;
  }
  return lhs.column <= rhs.column;
}

} // namespace neuron::sema_detail
