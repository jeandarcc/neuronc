#pragma once

#include "LspTypes.h"
#include "neuronc/diagnostics/DiagnosticLocale.h"
#include "neuronc/diagnostics/DiagnosticLocalizer.h"

#include <unordered_map>

namespace neuron::lsp {

class DocumentManager {
public:
  void setWorkspaceRoot(fs::path root);
  const fs::path &configuredWorkspaceRoot() const;
  void setToolRoot(fs::path root);
  const fs::path &toolRoot() const;
  void setDiagnosticLanguage(neuron::diagnostics::ResolvedLanguage language);
  const neuron::diagnostics::ResolvedLanguage &diagnosticLanguage() const;

  DocumentState &openDocument(std::string uri, std::string text, int version);
  DocumentState *applyChanges(const std::string &uri,
                              const std::vector<DocumentChange> &changes,
                              int version);
  bool closeDocument(const std::string &uri);

  DocumentState *find(const std::string &uri);
  const DocumentState *find(const std::string &uri) const;

  fs::path workspaceRootFor(const fs::path &path) const;
  fs::path effectiveWorkspaceRoot() const;

  void reanalyze(DocumentState &document);
  std::optional<std::string> readWorkspaceFile(const fs::path &path) const;
  std::vector<fs::path> collectWorkspaceFiles(const fs::path &workspaceRoot) const;
  std::optional<WorkspaceSemanticState>
  analyzeWorkspaceFile(const fs::path &path,
                       const fs::path &workspaceRoot) const;

private:
  fs::path m_workspaceRoot;
  fs::path m_toolRoot;
  neuron::diagnostics::ResolvedLanguage m_diagnosticLanguage;
  neuron::diagnostics::DiagnosticLocalizer m_localizer;
  std::unordered_map<std::string, DocumentState> m_documents;
};

} // namespace neuron::lsp
