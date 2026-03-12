#pragma once

#include "neuronc/diagnostics/DiagnosticFormat.h"
#include "neuronc/frontend/Diagnostics.h"

#include <filesystem>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace neuron::diagnostics {

struct LocalizedDiagnosticEntry {
  std::string code;
  std::string title;
  std::string summary;
  std::string defaultMessage;
  std::string recovery;
  std::vector<std::string> parameters;
};

class DiagnosticLocalizer {
public:
  explicit DiagnosticLocalizer(std::filesystem::path toolRoot = {});

  void setToolRoot(std::filesystem::path toolRoot);
  const std::filesystem::path &toolRoot() const;

  std::optional<LocalizedDiagnosticEntry>
  findEntry(const std::string &languageCode, const std::string &diagnosticCode);

  frontend::Diagnostic renderDiagnostic(const frontend::Diagnostic &diagnostic,
                                        const std::string &languageCode);

  std::vector<frontend::Diagnostic>
  localizeDiagnostics(const std::vector<frontend::Diagnostic> &diagnostics,
                      const std::string &languageCode);

private:
  using EntryMap = std::unordered_map<std::string, LocalizedDiagnosticEntry>;

  std::optional<std::reference_wrapper<const EntryMap>>
  loadLocaleEntries(const std::string &languageCode);

  std::filesystem::path m_toolRoot;
  std::unordered_map<std::string, EntryMap> m_cache;
};

} // namespace neuron::diagnostics

