#pragma once

#include "neuronc/sema/SemanticAnalyzer.h"

namespace neuron::sema_detail {

class ScopeManager {
public:
  void reset();
  void recordSnapshot(const SourceLocation &location,
                      std::vector<VisibleSymbolInfo> symbols);
  std::vector<VisibleSymbolInfo>
  snapshotAt(const SourceLocation &location) const;

private:
  struct ScopeSnapshotEntry {
    SourceLocation location;
    std::vector<VisibleSymbolInfo> symbols;
  };

  static bool isLocationAtOrBefore(const SourceLocation &lhs,
                                   const SourceLocation &rhs);

  std::vector<ScopeSnapshotEntry> m_scopeSnapshots;
};

} // namespace neuron::sema_detail
