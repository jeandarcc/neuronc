#pragma once

#include "LspTypes.h"

#include <memory>
#include <unordered_set>

namespace neuron::lsp::detail {

std::unordered_set<std::string> collectAvailableModules(const fs::path &root,
                                                        const fs::path &path);
std::vector<ResolvedWorkspaceSource>
resolveWorkspaceSources(const fs::path &entryPath, const fs::path &workspaceRoot,
                        const WorkspaceFileReader &reader);
std::unique_ptr<ProgramNode>
mergeResolvedPrograms(std::vector<std::unique_ptr<ProgramNode>> *programs,
                      const std::string &entryModule);
frontend::SemanticOptions makeSemanticOptions(const DocumentState &document,
                                              const fs::path &workspaceRoot);
frontend::SemanticOptions
makeWorkspaceSemanticOptions(const fs::path &entryPath,
                             const fs::path &workspaceRoot);
fs::path findWorkspaceRoot(const fs::path &path, const fs::path &fallback);

} // namespace neuron::lsp::detail
