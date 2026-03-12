#include "CallHierarchyHandler.h"

#include "lsp/LspAst.h"
#include "lsp/LspFeaturePayloads.h"
#include "lsp/LspHoverSupport.h"
#include "lsp/LspPath.h"
#include "lsp/LspProtocol.h"

#include <unordered_map>

namespace neuron::lsp {

namespace {

std::optional<SourceLocation> callCalleeLocation(const CallExprNode *call) {
  if (call == nullptr || call->callee == nullptr) {
    return std::nullopt;
  }

  switch (call->callee->type) {
  case ASTNodeType::Identifier:
  case ASTNodeType::TypeSpec:
    return call->callee->location;
  case ASTNodeType::MemberAccessExpr:
    return static_cast<const MemberAccessNode *>(call->callee.get())->memberLocation;
  default:
    return std::nullopt;
  }
}

std::string callCalleeName(const CallExprNode *call) {
  if (call == nullptr || call->callee == nullptr) {
    return "";
  }

  switch (call->callee->type) {
  case ASTNodeType::Identifier:
    return static_cast<const IdentifierNode *>(call->callee.get())->name;
  case ASTNodeType::TypeSpec:
    return static_cast<const TypeSpecNode *>(call->callee.get())->typeName;
  case ASTNodeType::MemberAccessExpr:
    return static_cast<const MemberAccessNode *>(call->callee.get())->member;
  default:
    return "";
  }
}

std::optional<VisibleSymbolInfo> resolveCallSymbol(const SemanticAnalyzer *analyzer,
                                                   const CallExprNode *call) {
  if (analyzer == nullptr) {
    return std::nullopt;
  }
  const std::optional<SourceLocation> location = callCalleeLocation(call);
  return location.has_value() ? analyzer->getResolvedSymbol(*location) : std::nullopt;
}

VisibleSymbolInfo fallbackMethodSymbol(const MethodDeclNode *method) {
  VisibleSymbolInfo info;
  if (method == nullptr) {
    return info;
  }
  info.name = method->name;
  info.kind = method->name == "constructor" ? SymbolKind::Constructor
                                             : SymbolKind::Method;
  info.definition = SymbolLocation{
      method->location, std::max(1, static_cast<int>(method->name.size()))};
  return info;
}

} // namespace

CallHierarchyHandler::CallHierarchyHandler(LspDispatcher &dispatcher,
                                           DocumentManager &documents,
                                           LspTransport &transport)
    : m_documents(documents), m_transport(transport) {
  dispatcher.registerHandler("textDocument/prepareCallHierarchy",
                             [this](const LspMessageContext &context) {
                               return handlePrepare(context);
                             });
  dispatcher.registerHandler("callHierarchy/incomingCalls",
                             [this](const LspMessageContext &context) {
                               return handleIncoming(context);
                             });
  dispatcher.registerHandler("callHierarchy/outgoingCalls",
                             [this](const LspMessageContext &context) {
                               return handleOutgoing(context);
                             });
}

DispatchResult CallHierarchyHandler::handlePrepare(
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
  if (!resolved.has_value() ||
      (resolved->kind != SymbolKind::Method &&
       resolved->kind != SymbolKind::Constructor &&
       resolved->kind != SymbolKind::Class)) {
    m_transport.sendResult(*context.id, llvm::json::Array{});
    return DispatchResult::Continue;
  }

  SymbolLocation definition =
      resolved->definition.value_or(SymbolLocation{
          lookupLocation, std::max(1, static_cast<int>(resolved->name.size()))});
  if (definition.location.file.empty()) {
    definition.location.file = document->path.string();
  }

  const fs::path definitionPath =
      detail::normalizePath(fs::path(definition.location.file));
  const fs::path workspaceRoot = m_documents.workspaceRootFor(document->path);
  const std::optional<WorkspaceSemanticState> state =
      m_documents.analyzeWorkspaceFile(definitionPath, workspaceRoot);
  if (!state.has_value() || state->semanticResult.analyzer == nullptr ||
      state->mergedProgram == nullptr) {
    m_transport.sendResult(*context.id, llvm::json::Array{});
    return DispatchResult::Continue;
  }

  VisibleSymbolInfo symbol = *resolved;
  if (const std::optional<VisibleSymbolInfo> definitionSymbol =
          state->semanticResult.analyzer->getResolvedSymbol(definition.location)) {
    symbol = *definitionSymbol;
  }
  symbol.definition = definition;

  llvm::json::Array items;
  items.push_back(detail::makeCallHierarchyItem(
      symbol, definition, state->mergedProgram.get(),
      state->semanticResult.analyzer.get()));
  m_transport.sendResult(*context.id, std::move(items));
  return DispatchResult::Continue;
}

DispatchResult CallHierarchyHandler::handleIncoming(
    const LspMessageContext &context) {
  if (context.id == nullptr) {
    return DispatchResult::Continue;
  }
  if (context.params == nullptr) {
    m_transport.sendResult(*context.id, llvm::json::Array{});
    return DispatchResult::Continue;
  }

  const std::optional<SymbolLocation> targetDefinition =
      detail::parseHierarchyItemLocation(context.params->getObject("item"));
  if (!targetDefinition.has_value()) {
    m_transport.sendResult(*context.id, llvm::json::Array{});
    return DispatchResult::Continue;
  }

  struct IncomingCallGroup {
    llvm::json::Object from;
    llvm::json::Array fromRanges;
  };

  const fs::path targetPath =
      detail::normalizePath(fs::path(targetDefinition->location.file));
  const fs::path workspaceRoot = m_documents.workspaceRootFor(targetPath);
  std::unordered_map<std::string, IncomingCallGroup> groupedCalls;

  for (const fs::path &file : m_documents.collectWorkspaceFiles(workspaceRoot)) {
    const std::optional<WorkspaceSemanticState> state =
        m_documents.analyzeWorkspaceFile(file, workspaceRoot);
    if (!state.has_value() || state->semanticResult.analyzer == nullptr ||
        state->mergedProgram == nullptr) {
      continue;
    }

    detail::walkAst(state->mergedProgram.get(), [&](const ASTNode *node) {
      if (node == nullptr || node->type != ASTNodeType::CallExpr ||
          !detail::samePath(node->location.file, file)) {
        return;
      }

      const auto *call = static_cast<const CallExprNode *>(node);
      const std::optional<VisibleSymbolInfo> targetSymbol =
          resolveCallSymbol(state->semanticResult.analyzer.get(), call);
      if (!targetSymbol.has_value() || !targetSymbol->definition.has_value() ||
          !detail::sameSymbolLocation(*targetSymbol->definition,
                                      *targetDefinition)) {
        return;
      }

      const auto *caller =
          detail::findEnclosingMethod(state->mergedProgram.get(), call->location);
      if (caller == nullptr) {
        return;
      }

      VisibleSymbolInfo callerSymbol = fallbackMethodSymbol(caller);
      if (const std::optional<VisibleSymbolInfo> resolvedCaller =
              state->semanticResult.analyzer->getResolvedSymbol(caller->location)) {
        callerSymbol = *resolvedCaller;
      }
      if (!callerSymbol.definition.has_value()) {
        callerSymbol.definition = SymbolLocation{
            caller->location, std::max(1, static_cast<int>(caller->name.size()))};
      }

      const std::string groupKey =
          detail::pathKey(fs::path(callerSymbol.definition->location.file)) + ":" +
          std::to_string(callerSymbol.definition->location.line) + ":" +
          std::to_string(callerSymbol.definition->location.column);
      auto groupIt = groupedCalls.find(groupKey);
      if (groupIt == groupedCalls.end()) {
        IncomingCallGroup group;
        group.from = detail::makeCallHierarchyItem(
            callerSymbol, *callerSymbol.definition, state->mergedProgram.get(),
            state->semanticResult.analyzer.get());
        groupIt = groupedCalls.emplace(groupKey, std::move(group)).first;
      }

      const std::optional<SourceLocation> location = callCalleeLocation(call);
      if (!location.has_value()) {
        return;
      }
      const SymbolRange callRange = detail::makePointSymbolRange(
          *location, std::max(1, static_cast<int>(callCalleeName(call).size())));
      groupIt->second.fromRanges.push_back(detail::makeLspRange(callRange));
    });
  }

  llvm::json::Array result;
  for (auto &entry : groupedCalls) {
    llvm::json::Object callObject;
    callObject["from"] = std::move(entry.second.from);
    callObject["fromRanges"] = std::move(entry.second.fromRanges);
    result.push_back(std::move(callObject));
  }
  m_transport.sendResult(*context.id, std::move(result));
  return DispatchResult::Continue;
}

DispatchResult CallHierarchyHandler::handleOutgoing(
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
  const std::optional<WorkspaceSemanticState> state =
      m_documents.analyzeWorkspaceFile(definitionPath, workspaceRoot);
  if (!state.has_value() || state->semanticResult.analyzer == nullptr ||
      state->mergedProgram == nullptr) {
    m_transport.sendResult(*context.id, llvm::json::Array{});
    return DispatchResult::Continue;
  }

  const auto *method =
      detail::findMethodByLocation(state->mergedProgram.get(), definition->location);
  if (method == nullptr || method->body == nullptr) {
    m_transport.sendResult(*context.id, llvm::json::Array{});
    return DispatchResult::Continue;
  }

  struct OutgoingCallGroup {
    llvm::json::Object to;
    llvm::json::Array fromRanges;
  };

  std::unordered_map<std::string, OutgoingCallGroup> groupedCalls;
  detail::walkAst(method->body.get(), [&](const ASTNode *node) {
    if (node == nullptr || node->type != ASTNodeType::CallExpr ||
        !detail::samePath(node->location.file, definitionPath)) {
      return;
    }

    const auto *call = static_cast<const CallExprNode *>(node);
    const std::optional<VisibleSymbolInfo> targetSymbol =
        resolveCallSymbol(state->semanticResult.analyzer.get(), call);
    if (!targetSymbol.has_value() ||
        (targetSymbol->kind != SymbolKind::Method &&
         targetSymbol->kind != SymbolKind::Constructor &&
         targetSymbol->kind != SymbolKind::Class)) {
      return;
    }

    SymbolLocation targetDefinition =
        targetSymbol->definition.value_or(SymbolLocation{
            callCalleeLocation(call).value_or(call->location),
            std::max(1, static_cast<int>(callCalleeName(call).size()))});
    if (targetDefinition.location.file.empty()) {
      targetDefinition.location.file = definitionPath.string();
    }

    VisibleSymbolInfo symbol = *targetSymbol;
    symbol.definition = targetDefinition;

    const fs::path targetPath =
        detail::normalizePath(fs::path(targetDefinition.location.file));
    const std::optional<WorkspaceSemanticState> targetState =
        m_documents.analyzeWorkspaceFile(targetPath, workspaceRoot);
    const ASTNode *targetRoot =
        targetState.has_value() ? targetState->mergedProgram.get() : nullptr;
    const SemanticAnalyzer *targetAnalyzer =
        targetState.has_value() ? targetState->semanticResult.analyzer.get() : nullptr;

    const std::string groupKey = detail::pathKey(targetPath) + ":" +
                                 std::to_string(targetDefinition.location.line) +
                                 ":" +
                                 std::to_string(targetDefinition.location.column);
    auto groupIt = groupedCalls.find(groupKey);
    if (groupIt == groupedCalls.end()) {
      OutgoingCallGroup group;
      group.to = detail::makeCallHierarchyItem(symbol, targetDefinition,
                                               targetRoot, targetAnalyzer);
      groupIt = groupedCalls.emplace(groupKey, std::move(group)).first;
    }

    const std::optional<SourceLocation> location = callCalleeLocation(call);
    if (!location.has_value()) {
      return;
    }
    const SymbolRange callRange = detail::makePointSymbolRange(
        *location, std::max(1, static_cast<int>(callCalleeName(call).size())));
    groupIt->second.fromRanges.push_back(detail::makeLspRange(callRange));
  });

  llvm::json::Array result;
  for (auto &entry : groupedCalls) {
    llvm::json::Object callObject;
    callObject["to"] = std::move(entry.second.to);
    callObject["fromRanges"] = std::move(entry.second.fromRanges);
    result.push_back(std::move(callObject));
  }
  m_transport.sendResult(*context.id, std::move(result));
  return DispatchResult::Continue;
}

} // namespace neuron::lsp
