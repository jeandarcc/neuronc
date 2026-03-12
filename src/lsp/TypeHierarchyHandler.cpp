#include "TypeHierarchyHandler.h"

#include "lsp/LspFeaturePayloads.h"
#include "lsp/LspHoverSupport.h"
#include "lsp/LspPath.h"
#include "lsp/LspSymbols.h"

#include <algorithm>
#include <unordered_set>

namespace neuron::lsp {

TypeHierarchyHandler::TypeHierarchyHandler(LspDispatcher &dispatcher,
                                           DocumentManager &documents,
                                           LspTransport &transport)
    : m_documents(documents), m_transport(transport) {
  dispatcher.registerHandler("textDocument/prepareTypeHierarchy",
                             [this](const LspMessageContext &context) {
                               return handlePrepare(context);
                             });
  dispatcher.registerHandler("typeHierarchy/supertypes",
                             [this](const LspMessageContext &context) {
                               return handleSupertypes(context);
                             });
  dispatcher.registerHandler("typeHierarchy/subtypes",
                             [this](const LspMessageContext &context) {
                               return handleSubtypes(context);
                             });
}

DispatchResult TypeHierarchyHandler::handlePrepare(
    const LspMessageContext &context) {
  if (context.id == nullptr) {
    return DispatchResult::Continue;
  }
  if (context.params == nullptr) {
    m_transport.sendResult(*context.id, llvm::json::Array{});
    return DispatchResult::Continue;
  }

  const auto *textDocument = context.params->getObject("textDocument");
  const auto *position = context.params->getObject("position");
  if (textDocument == nullptr || position == nullptr) {
    m_transport.sendResult(*context.id, llvm::json::Array{});
    return DispatchResult::Continue;
  }

  const std::optional<llvm::StringRef> uri = textDocument->getString("uri");
  const DocumentState *document =
      uri.has_value() ? m_documents.find(std::string(uri->str())) : nullptr;
  if (document == nullptr || document->analyzer == nullptr) {
    m_transport.sendResult(*context.id, llvm::json::Array{});
    return DispatchResult::Continue;
  }

  const int line = static_cast<int>(position->getInteger("line").value_or(0));
  const int character =
      static_cast<int>(position->getInteger("character").value_or(0));
  SourceLocation lookupLocation;
  const ASTNode *node = nullptr;
  const std::optional<VisibleSymbolInfo> resolved =
      detail::resolveSymbolAtPosition(*document, line, character, &lookupLocation,
                                      &node);
  if (!resolved.has_value() || resolved->kind != SymbolKind::Class) {
    m_transport.sendResult(*context.id, llvm::json::Array{});
    return DispatchResult::Continue;
  }

  SymbolLocation definition =
      resolved->definition.value_or(SymbolLocation{
          lookupLocation, std::max(1, static_cast<int>(resolved->name.size()))});
  if (definition.location.file.empty()) {
    definition.location.file = document->path.string();
  }

  const fs::path workspaceRoot = m_documents.workspaceRootFor(document->path);
  const std::vector<WorkspaceTypeRecord> records = detail::collectWorkspaceTypes(
      m_documents.collectWorkspaceFiles(workspaceRoot),
      [&](const fs::path &file) { return m_documents.readWorkspaceFile(file); });

  auto recordIt = std::find_if(records.begin(), records.end(),
                               [&](const WorkspaceTypeRecord &record) {
                                 return detail::samePath(record.path,
                                                         definition.location.file) &&
                                        detail::sameLocation(record.location,
                                                             definition.location);
                               });
  if (recordIt == records.end()) {
    m_transport.sendResult(*context.id, llvm::json::Array{});
    return DispatchResult::Continue;
  }

  llvm::json::Array items;
  items.push_back(detail::makeTypeHierarchyItem(*recordIt));
  m_transport.sendResult(*context.id, std::move(items));
  return DispatchResult::Continue;
}

DispatchResult TypeHierarchyHandler::handleSupertypes(
    const LspMessageContext &context) {
  if (context.id == nullptr) {
    return DispatchResult::Continue;
  }
  if (context.params == nullptr) {
    m_transport.sendResult(*context.id, llvm::json::Array{});
    return DispatchResult::Continue;
  }

  const std::optional<SymbolLocation> definition =
      detail::parseHierarchyItemLocation(context.params->getObject("item"));
  if (!definition.has_value()) {
    m_transport.sendResult(*context.id, llvm::json::Array{});
    return DispatchResult::Continue;
  }

  const fs::path definitionPath =
      detail::normalizePath(fs::path(definition->location.file));
  const fs::path workspaceRoot = m_documents.workspaceRootFor(definitionPath);
  const std::vector<WorkspaceTypeRecord> records = detail::collectWorkspaceTypes(
      m_documents.collectWorkspaceFiles(workspaceRoot),
      [&](const fs::path &file) { return m_documents.readWorkspaceFile(file); });

  const auto currentIt = std::find_if(records.begin(), records.end(),
                                      [&](const WorkspaceTypeRecord &record) {
                                        return detail::samePath(record.path,
                                                                definitionPath) &&
                                               detail::sameLocation(record.location,
                                                                    definition->location);
                                      });
  if (currentIt == records.end()) {
    m_transport.sendResult(*context.id, llvm::json::Array{});
    return DispatchResult::Continue;
  }

  llvm::json::Array result;
  std::unordered_set<std::string> emitted;
  for (const std::string &baseName : currentIt->baseClasses) {
    for (const WorkspaceTypeRecord &record : records) {
      if (record.name != baseName) {
        continue;
      }
      const std::string key = detail::pathKey(record.path) + ":" +
                              std::to_string(record.location.line) + ":" +
                              std::to_string(record.location.column);
      if (!emitted.insert(key).second) {
        continue;
      }
      result.push_back(detail::makeTypeHierarchyItem(record));
    }
  }
  m_transport.sendResult(*context.id, std::move(result));
  return DispatchResult::Continue;
}

DispatchResult TypeHierarchyHandler::handleSubtypes(
    const LspMessageContext &context) {
  if (context.id == nullptr) {
    return DispatchResult::Continue;
  }
  if (context.params == nullptr) {
    m_transport.sendResult(*context.id, llvm::json::Array{});
    return DispatchResult::Continue;
  }

  const std::optional<SymbolLocation> definition =
      detail::parseHierarchyItemLocation(context.params->getObject("item"));
  if (!definition.has_value()) {
    m_transport.sendResult(*context.id, llvm::json::Array{});
    return DispatchResult::Continue;
  }

  const fs::path definitionPath =
      detail::normalizePath(fs::path(definition->location.file));
  const fs::path workspaceRoot = m_documents.workspaceRootFor(definitionPath);
  const std::vector<WorkspaceTypeRecord> records = detail::collectWorkspaceTypes(
      m_documents.collectWorkspaceFiles(workspaceRoot),
      [&](const fs::path &file) { return m_documents.readWorkspaceFile(file); });

  auto currentIt = std::find_if(records.begin(), records.end(),
                                [&](const WorkspaceTypeRecord &record) {
                                  return detail::samePath(record.path,
                                                          definitionPath) &&
                                         detail::sameLocation(record.location,
                                                              definition->location);
                                });
  if (currentIt == records.end()) {
    m_transport.sendResult(*context.id, llvm::json::Array{});
    return DispatchResult::Continue;
  }

  llvm::json::Array result;
  for (const WorkspaceTypeRecord &record : records) {
    if (std::find(record.baseClasses.begin(), record.baseClasses.end(),
                  currentIt->name) == record.baseClasses.end()) {
      continue;
    }
    result.push_back(detail::makeTypeHierarchyItem(record));
  }
  m_transport.sendResult(*context.id, std::move(result));
  return DispatchResult::Continue;
}

} // namespace neuron::lsp
