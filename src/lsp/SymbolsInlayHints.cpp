#include "SymbolsHandler.h"

#include "lsp/LspAst.h"
#include "lsp/LspPath.h"
#include "lsp/LspProtocol.h"

#include <limits>
#include <unordered_set>

namespace neuron::lsp {

namespace {

bool isRenderableType(const NTypePtr &type) {
  return type != nullptr && !type->isUnknown() && !type->isError() &&
         !type->isAuto();
}

std::string truncateHintText(std::string text, std::size_t maxLength = 40) {
  if (text.size() <= maxLength) {
    return text;
  }
  if (maxLength <= 3) {
    return text.substr(0, maxLength);
  }
  return text.substr(0, maxLength - 3) + "...";
}

} // namespace

DispatchResult SymbolsHandler::handleInlayHints(
    const LspMessageContext &context) {
  if (context.id == nullptr) {
    return DispatchResult::Continue;
  }
  if (context.params == nullptr) {
    m_transport.sendResult(*context.id, llvm::json::Array{});
    return DispatchResult::Continue;
  }

  const auto *textDocument = context.params->getObject("textDocument");
  if (textDocument == nullptr) {
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

  frontend::Range requestedRange;
  requestedRange.start = {1, 1};
  requestedRange.end = {std::numeric_limits<int>::max(),
                        std::numeric_limits<int>::max()};
  if (const auto *rangeObject = context.params->getObject("range")) {
    requestedRange = detail::parseLspRange(rangeObject);
  }

  llvm::json::Array hints;
  std::unordered_set<std::string> emitted;
  const auto appendRawHint =
      [&](const SourceLocation &location, int length, std::string label, int kind,
          std::string tooltip) {
        if (label.empty() || !detail::samePath(location.file, document->path)) {
          return;
        }

        frontend::Position position = {
            location.line, location.column + std::max(1, length)};
        if (!detail::positionWithinRange(position, requestedRange)) {
          return;
        }

        const std::string key = location.file + ":" +
                                std::to_string(position.line) + ":" +
                                std::to_string(position.column) + ":" + label;
        if (!emitted.insert(key).second) {
          return;
        }

        llvm::json::Object hint;
        hint["position"] = llvm::json::Object{
            {"line", std::max(0, position.line - 1)},
            {"character", std::max(0, position.column - 1)}};
        hint["label"] = std::move(label);
        hint["kind"] = kind;
        hint["paddingLeft"] = true;
        if (!tooltip.empty()) {
          hint["tooltip"] = std::move(tooltip);
        }
        hints.push_back(std::move(hint));
      };

  const auto appendTypeHint = [&](const SourceLocation &location,
                                  std::string_view name, const NTypePtr &type) {
    if (!isRenderableType(type) || name.empty()) {
      return;
    }
    appendRawHint(location, static_cast<int>(name.size()), ": " + type->toString(),
                  1, "");
  };

  for (const auto &record : document->macroExpansions) {
    appendRawHint(record.location, record.length,
                  " => " + truncateHintText(record.expansion), 2,
                  record.qualifiedName.empty()
                      ? record.expansion
                      : (record.qualifiedName + " => " + record.expansion));
  }

  for (const auto &record : document->ownershipHints) {
    appendRawHint(record.location, record.length, record.label, 2, record.tooltip);
  }

  for (const auto &chunk : document->chunks) {
    if (chunk.parseResult.program == nullptr) {
      continue;
    }
    detail::walkAst(chunk.parseResult.program.get(), [&](const ASTNode *node) {
      if (node == nullptr || !detail::samePath(node->location.file, document->path)) {
        return;
      }

      switch (node->type) {
      case ASTNodeType::BindingDecl: {
        const auto *binding = static_cast<const BindingDeclNode *>(node);
        if (binding->name.empty() || binding->name == "__assign__" ||
            binding->name == "__deref__") {
          return;
        }

        bool shouldEmit = binding->typeAnnotation == nullptr;
        if (!shouldEmit && binding->typeAnnotation->type == ASTNodeType::TypeSpec) {
          shouldEmit =
              static_cast<const TypeSpecNode *>(binding->typeAnnotation.get())
                  ->typeName == "auto";
        }
        if (shouldEmit) {
          appendTypeHint(binding->location, binding->name,
                         document->analyzer->getInferredType(node));
        }
        return;
      }
      case ASTNodeType::ForInStmt: {
        const auto *forIn = static_cast<const ForInStmtNode *>(node);
        const std::optional<VisibleSymbolInfo> symbol =
            document->analyzer->getResolvedSymbol(forIn->variableLocation);
        appendTypeHint(forIn->variableLocation, forIn->variable,
                       symbol.has_value() ? symbol->type : nullptr);
        return;
      }
      case ASTNodeType::CatchClause: {
        const auto *catchClause = static_cast<const CatchClauseNode *>(node);
        if (catchClause->errorType != nullptr || catchClause->errorName.empty()) {
          return;
        }
        const std::optional<VisibleSymbolInfo> symbol =
            document->analyzer->getResolvedSymbol(catchClause->errorLocation);
        appendTypeHint(catchClause->errorLocation, catchClause->errorName,
                       symbol.has_value() ? symbol->type : nullptr);
        return;
      }
      default:
        return;
      }
    });
  }

  m_transport.sendResult(*context.id, std::move(hints));
  return DispatchResult::Continue;
}

} // namespace neuron::lsp
