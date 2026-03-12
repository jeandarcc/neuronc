#include "SymbolsHandler.h"

#include "lsp/LspAst.h"
#include "lsp/LspFeaturePayloads.h"
#include "lsp/LspPath.h"

#include <algorithm>

namespace neuron::lsp {

SymbolsHandler::SymbolsHandler(LspDispatcher &dispatcher,
                               DocumentManager &documents,
                               LspTransport &transport)
    : m_documents(documents), m_transport(transport) {
  dispatcher.registerHandler("textDocument/inlayHint",
                             [this](const LspMessageContext &context) {
                               return handleInlayHints(context);
                             });
  dispatcher.registerHandler("textDocument/semanticTokens/full",
                             [this](const LspMessageContext &context) {
                               return handleSemanticTokens(context);
                             });
  dispatcher.registerHandler("textDocument/documentSymbol",
                             [this](const LspMessageContext &context) {
                               return handleDocumentSymbols(context);
                             });
}

DispatchResult SymbolsHandler::handleSemanticTokens(
    const LspMessageContext &context) {
  if (context.id == nullptr) {
    return DispatchResult::Continue;
  }
  if (context.params == nullptr) {
    m_transport.sendResult(*context.id,
                           llvm::json::Object{{"data", llvm::json::Array{}}});
    return DispatchResult::Continue;
  }

  const auto *textDocument = context.params->getObject("textDocument");
  if (textDocument == nullptr) {
    m_transport.sendResult(*context.id,
                           llvm::json::Object{{"data", llvm::json::Array{}}});
    return DispatchResult::Continue;
  }

  const std::optional<llvm::StringRef> uri = textDocument->getString("uri");
  const DocumentState *document =
      uri.has_value() ? m_documents.find(std::string(uri->str())) : nullptr;
  if (document == nullptr || document->analyzer == nullptr) {
    m_transport.sendResult(*context.id,
                           llvm::json::Object{{"data", llvm::json::Array{}}});
    return DispatchResult::Continue;
  }

  std::vector<SemanticTokenEntry> tokens;
  const auto appendResolvedToken = [&](const SourceLocation &location, int length,
                                       const std::optional<VisibleSymbolInfo> &symbol) {
    if (!symbol.has_value() || !detail::samePath(location.file, document->path)) {
      return;
    }
    const std::optional<int> tokenType = detail::toSemanticTokenType(symbol->kind);
    if (!tokenType.has_value()) {
      return;
    }
    detail::appendSemanticToken(&tokens, std::max(0, location.line - 1),
                                std::max(0, location.column - 1), length,
                                *tokenType);
  };

  for (const auto &chunk : document->chunks) {
    if (chunk.parseResult.program == nullptr) {
      continue;
    }
    detail::walkAst(chunk.parseResult.program.get(), [&](const ASTNode *node) {
      if (node == nullptr || !detail::samePath(node->location.file, document->path)) {
        return;
      }

      switch (node->type) {
      case ASTNodeType::ModuleDecl: {
        const auto *moduleDecl = static_cast<const ModuleDeclNode *>(node);
        detail::appendSemanticToken(
            &tokens, std::max(0, moduleDecl->location.line - 1),
            std::max(0, moduleDecl->location.column - 1),
            std::max(1, static_cast<int>(moduleDecl->moduleName.size())),
            static_cast<int>(LspSemanticTokenType::Namespace));
        return;
      }
      case ASTNodeType::ModuleCppDecl: {
        const auto *moduleDecl = static_cast<const ModuleCppDeclNode *>(node);
        detail::appendSemanticToken(
            &tokens, std::max(0, moduleDecl->location.line - 1),
            std::max(0, moduleDecl->location.column - 1),
            std::max(1, static_cast<int>(moduleDecl->moduleName.size())),
            static_cast<int>(LspSemanticTokenType::Namespace));
        return;
      }
      case ASTNodeType::ClassDecl: {
        const auto *classDecl = static_cast<const ClassDeclNode *>(node);
        detail::appendSemanticToken(
            &tokens, std::max(0, classDecl->location.line - 1),
            std::max(0, classDecl->location.column - 1),
            std::max(1, static_cast<int>(classDecl->name.size())),
            static_cast<int>(LspSemanticTokenType::Type));
        return;
      }
      case ASTNodeType::EnumDecl: {
        const auto *enumDecl = static_cast<const EnumDeclNode *>(node);
        detail::appendSemanticToken(
            &tokens, std::max(0, enumDecl->location.line - 1),
            std::max(0, enumDecl->location.column - 1),
            std::max(1, static_cast<int>(enumDecl->name.size())),
            static_cast<int>(LspSemanticTokenType::Type));
        return;
      }
      case ASTNodeType::ShaderDecl: {
        const auto *shaderDecl = static_cast<const ShaderDeclNode *>(node);
        detail::appendSemanticToken(
            &tokens, std::max(0, shaderDecl->location.line - 1),
            std::max(0, shaderDecl->location.column - 1),
            std::max(1, static_cast<int>(shaderDecl->name.size())),
            static_cast<int>(LspSemanticTokenType::Type));
        return;
      }
      case ASTNodeType::MethodDecl: {
        const auto *method = static_cast<const MethodDeclNode *>(node);
        appendResolvedToken(method->location,
                            std::max(1, static_cast<int>(method->name.size())),
                            document->analyzer->getResolvedSymbol(method->location));
        for (const auto &parameter : method->parameters) {
          detail::appendSemanticToken(
              &tokens, std::max(0, parameter.location.line - 1),
              std::max(0, parameter.location.column - 1),
              std::max(1, static_cast<int>(parameter.name.size())),
              static_cast<int>(LspSemanticTokenType::Parameter));
        }
        return;
      }
      case ASTNodeType::BindingDecl: {
        const auto *binding = static_cast<const BindingDeclNode *>(node);
        if (binding->name.empty() || binding->name == "__assign__" ||
            binding->name == "__deref__") {
          return;
        }
        appendResolvedToken(binding->location,
                            std::max(1, static_cast<int>(binding->name.size())),
                            document->analyzer->getResolvedSymbol(binding->location));
        return;
      }
      case ASTNodeType::TypeSpec: {
        const auto *typeSpec = static_cast<const TypeSpecNode *>(node);
        detail::appendSemanticToken(
            &tokens, std::max(0, typeSpec->location.line - 1),
            std::max(0, typeSpec->location.column - 1),
            std::max(1, static_cast<int>(typeSpec->typeName.size())),
            static_cast<int>(LspSemanticTokenType::Type));
        return;
      }
      case ASTNodeType::Identifier: {
        const auto *identifier = static_cast<const IdentifierNode *>(node);
        if (identifier->name == "__missing__") {
          return;
        }
        appendResolvedToken(
            identifier->location,
            std::max(1, static_cast<int>(identifier->name.size())),
            document->analyzer->getResolvedSymbol(identifier->location));
        return;
      }
      case ASTNodeType::MemberAccessExpr: {
        const auto *member = static_cast<const MemberAccessNode *>(node);
        appendResolvedToken(
            member->memberLocation,
            std::max(1, static_cast<int>(member->member.size())),
            document->analyzer->getResolvedSymbol(member->memberLocation));
        return;
      }
      case ASTNodeType::ForInStmt: {
        const auto *forIn = static_cast<const ForInStmtNode *>(node);
        detail::appendSemanticToken(
            &tokens, std::max(0, forIn->variableLocation.line - 1),
            std::max(0, forIn->variableLocation.column - 1),
            std::max(1, static_cast<int>(forIn->variable.size())),
            static_cast<int>(LspSemanticTokenType::Variable));
        return;
      }
      case ASTNodeType::CatchClause: {
        const auto *catchClause = static_cast<const CatchClauseNode *>(node);
        if (catchClause->errorName.empty()) {
          return;
        }
        detail::appendSemanticToken(
            &tokens, std::max(0, catchClause->errorLocation.line - 1),
            std::max(0, catchClause->errorLocation.column - 1),
            std::max(1, static_cast<int>(catchClause->errorName.size())),
            static_cast<int>(LspSemanticTokenType::Variable));
        return;
      }
      default:
        return;
      }
    });
  }

  m_transport.sendResult(*context.id, detail::encodeSemanticTokens(std::move(tokens)));
  return DispatchResult::Continue;
}

DispatchResult SymbolsHandler::handleDocumentSymbols(
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

  llvm::json::Array symbols;
  for (const auto &symbol : document->analyzer->getDocumentSymbols()) {
    symbols.push_back(detail::makeLspDocumentSymbol(symbol));
  }
  m_transport.sendResult(*context.id, std::move(symbols));
  return DispatchResult::Continue;
}

} // namespace neuron::lsp
