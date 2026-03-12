#pragma once

#include "LspTypes.h"

namespace neuron::lsp::detail {

std::optional<DocumentSymbolInfo>
buildWorkspaceDocumentSymbol(const ASTNode *node, bool classMember);
std::vector<WorkspaceSymbolEntry>
collectWorkspaceSymbolsForProgram(const ProgramNode *program, const fs::path &path);
std::vector<WorkspaceTypeRecord>
collectWorkspaceTypesForProgram(const ProgramNode *program, const fs::path &path);
std::vector<WorkspaceSymbolEntry>
collectWorkspaceSymbols(const std::vector<fs::path> &files,
                        const WorkspaceFileReader &reader);
std::vector<WorkspaceTypeRecord>
collectWorkspaceTypes(const std::vector<fs::path> &files,
                      const WorkspaceFileReader &reader);

} // namespace neuron::lsp::detail
