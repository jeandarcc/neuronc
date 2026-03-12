#include "LspSymbols.h"

#include "LspAst.h"
#include "LspPath.h"

#include <iterator>

namespace neuron::lsp::detail {

namespace {

void flattenDocumentSymbols(const DocumentSymbolInfo &symbol,
                            const std::string &containerName,
                            std::vector<WorkspaceSymbolEntry> *out) {
  if (out == nullptr) {
    return;
  }

  out->push_back(WorkspaceSymbolEntry{symbol.name, symbol.kind, symbol.range,
                                      containerName});
  for (const auto &child : symbol.children) {
    flattenDocumentSymbols(child, symbol.name, out);
  }
}

} // namespace

std::optional<DocumentSymbolInfo>
buildWorkspaceDocumentSymbol(const ASTNode *node, bool classMember) {
  if (node == nullptr) {
    return std::nullopt;
  }

  const auto makeRange = [](const SourceLocation &location,
                            std::string_view name) -> SymbolRange {
    return makePointSymbolRange(location, static_cast<int>(name.size()));
  };

  DocumentSymbolInfo info;
  switch (node->type) {
  case ASTNodeType::ClassDecl: {
    const auto *classDecl = static_cast<const ClassDeclNode *>(node);
    info.name = classDecl->name;
    info.kind = SymbolKind::Class;
    info.range = makeRange(classDecl->location, classDecl->name);
    info.selectionRange = info.range;
    for (const auto &member : classDecl->members) {
      if (auto child = buildWorkspaceDocumentSymbol(member.get(), true)) {
        info.children.push_back(std::move(*child));
      }
    }
    return info;
  }
  case ASTNodeType::MethodDecl: {
    const auto *method = static_cast<const MethodDeclNode *>(node);
    info.name = method->name;
    info.kind = classMember && method->name == "constructor"
                    ? SymbolKind::Constructor
                    : SymbolKind::Method;
    info.range = makeRange(method->location, method->name);
    info.selectionRange = info.range;
    return info;
  }
  case ASTNodeType::EnumDecl: {
    const auto *enumDecl = static_cast<const EnumDeclNode *>(node);
    info.name = enumDecl->name;
    info.kind = SymbolKind::Enum;
    info.range = makeRange(enumDecl->location, enumDecl->name);
    info.selectionRange = info.range;
    return info;
  }
  case ASTNodeType::ShaderDecl: {
    const auto *shaderDecl = static_cast<const ShaderDeclNode *>(node);
    info.name = shaderDecl->name;
    info.kind = SymbolKind::Shader;
    info.range = makeRange(shaderDecl->location, shaderDecl->name);
    info.selectionRange = info.range;
    return info;
  }
  case ASTNodeType::ModuleDecl: {
    const auto *moduleDecl = static_cast<const ModuleDeclNode *>(node);
    info.name = moduleDecl->moduleName;
    info.kind = SymbolKind::Module;
    info.range = makeRange(moduleDecl->location, moduleDecl->moduleName);
    info.selectionRange = info.range;
    return info;
  }
  case ASTNodeType::ModuleCppDecl: {
    const auto *moduleDecl = static_cast<const ModuleCppDeclNode *>(node);
    info.name = moduleDecl->moduleName;
    info.kind = SymbolKind::Module;
    info.range = makeRange(moduleDecl->location, moduleDecl->moduleName);
    info.selectionRange = info.range;
    return info;
  }
  case ASTNodeType::BindingDecl: {
    const auto *binding = static_cast<const BindingDeclNode *>(node);
    if (binding->name == "__assign__" || binding->name == "__deref__") {
      return std::nullopt;
    }
    info.name = binding->name;
    info.kind = classMember ? SymbolKind::Field : SymbolKind::Variable;
    info.range = makeRange(binding->location, binding->name);
    info.selectionRange = info.range;
    return info;
  }
  default:
    return std::nullopt;
  }
}

std::vector<WorkspaceSymbolEntry>
collectWorkspaceSymbolsForProgram(const ProgramNode *program, const fs::path &path) {
  std::vector<WorkspaceSymbolEntry> symbols;
  if (program == nullptr) {
    return symbols;
  }

  for (const auto &decl : program->declarations) {
    if (auto info = buildWorkspaceDocumentSymbol(decl.get(), false)) {
      if (!samePath(info->range.start.file, path)) {
        continue;
      }
      flattenDocumentSymbols(*info, "", &symbols);
    }
  }
  return symbols;
}

std::vector<WorkspaceTypeRecord>
collectWorkspaceTypesForProgram(const ProgramNode *program, const fs::path &path) {
  std::vector<WorkspaceTypeRecord> records;
  if (program == nullptr) {
    return records;
  }

  walkAst(program, [&](const ASTNode *node) {
    if (node == nullptr || node->type != ASTNodeType::ClassDecl ||
        !samePath(node->location.file, path)) {
      return;
    }
    const auto *classDecl = static_cast<const ClassDeclNode *>(node);
    records.push_back(WorkspaceTypeRecord{classDecl->name, classDecl->kind, path,
                                          classDecl->location,
                                          classDecl->baseClasses});
  });
  return records;
}

std::vector<WorkspaceSymbolEntry>
collectWorkspaceSymbols(const std::vector<fs::path> &files,
                        const WorkspaceFileReader &reader) {
  std::vector<WorkspaceSymbolEntry> symbols;
  for (const fs::path &file : files) {
    const std::optional<std::string> source = reader(file);
    if (!source.has_value()) {
      continue;
    }

    frontend::ParseResult parseResult =
        frontend::lexAndParseSource(*source, file.string());
    if (parseResult.program == nullptr) {
      continue;
    }

    std::vector<WorkspaceSymbolEntry> fileSymbols =
        collectWorkspaceSymbolsForProgram(parseResult.program.get(), file);
    symbols.insert(symbols.end(), std::make_move_iterator(fileSymbols.begin()),
                   std::make_move_iterator(fileSymbols.end()));
  }
  return symbols;
}

std::vector<WorkspaceTypeRecord>
collectWorkspaceTypes(const std::vector<fs::path> &files,
                      const WorkspaceFileReader &reader) {
  std::vector<WorkspaceTypeRecord> records;
  for (const fs::path &file : files) {
    const std::optional<std::string> source = reader(file);
    if (!source.has_value()) {
      continue;
    }

    frontend::ParseResult parseResult =
        frontend::lexAndParseSource(*source, file.string());
    if (parseResult.program == nullptr) {
      continue;
    }

    std::vector<WorkspaceTypeRecord> fileRecords =
        collectWorkspaceTypesForProgram(parseResult.program.get(), file);
    records.insert(records.end(), std::make_move_iterator(fileRecords.begin()),
                   std::make_move_iterator(fileRecords.end()));
  }
  return records;
}

} // namespace neuron::lsp::detail
