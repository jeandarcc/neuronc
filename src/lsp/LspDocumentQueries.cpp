#include "LspDocumentQueries.h"

#include "LspAst.h"
#include "LspPath.h"
#include "LspProtocol.h"

#include <unordered_set>

namespace neuron::lsp::detail {

const BindingDeclNode *findBindingInDocument(const DocumentState &document,
                                             const SourceLocation &location) {
  for (const auto &chunk : document.chunks) {
    if (chunk.parseResult.program == nullptr) {
      continue;
    }
    if (const auto *binding =
            findBindingByLocation(chunk.parseResult.program.get(), location)) {
      return binding;
    }
  }
  return nullptr;
}

EnclosingMethodContext findEnclosingMethodContext(const DocumentState &document,
                                                  const SourceLocation &location) {
  for (const auto &chunk : document.chunks) {
    if (chunk.parseResult.program == nullptr) {
      continue;
    }
    if (const auto *method =
            findEnclosingMethod(chunk.parseResult.program.get(), location)) {
      return {chunk.parseResult.program.get(), method,
              findOwningClass(chunk.parseResult.program.get(), method)};
    }
  }
  return {};
}

const CallExprNode *findCallInDocument(const DocumentState &document,
                                       const SourceLocation &location,
                                       EnclosingMethodContext *outContext) {
  for (const auto &chunk : document.chunks) {
    if (chunk.parseResult.program == nullptr) {
      continue;
    }
    if (const auto *call =
            findCallByCalleeLocation(chunk.parseResult.program.get(), location)) {
      if (outContext != nullptr) {
        const auto *method =
            findEnclosingMethod(chunk.parseResult.program.get(), location);
        *outContext = {chunk.parseResult.program.get(), method,
                       findOwningClass(chunk.parseResult.program.get(), method)};
      }
      return call;
    }
  }
  return nullptr;
}

frontend::Diagnostic makeUnusedVariableDiagnostic(const BindingDeclNode *binding) {
  frontend::Diagnostic diagnostic;
  diagnostic.phase = "semantic";
  diagnostic.severity = frontend::DiagnosticSeverity::Warning;
  diagnostic.code = std::string(kUnusedVariableDiagnosticCode);
  diagnostic.file = binding != nullptr ? binding->location.file : "";
  diagnostic.range =
      makeFrontendRange(binding != nullptr ? binding->location.line : 1,
                        binding != nullptr ? binding->location.column : 1,
                        binding != nullptr ? binding->location.line : 1,
                        (binding != nullptr ? binding->location.column : 1) +
                            (binding != nullptr
                                 ? std::max(1, static_cast<int>(binding->name.size()))
                                 : 1));
  diagnostic.message = std::string(kUnusedVariableDiagnosticPrefix) +
                       (binding != nullptr ? binding->name : "");
  return diagnostic;
}

std::vector<frontend::Diagnostic>
collectUnusedVariableDiagnostics(const DocumentState &document) {
  std::vector<frontend::Diagnostic> diagnostics;
  if (document.analyzer == nullptr) {
    return diagnostics;
  }

  std::unordered_set<std::string> emitted;
  for (const auto &chunk : document.chunks) {
    if (chunk.parseResult.program == nullptr) {
      continue;
    }

    walkAst(chunk.parseResult.program.get(), [&](const ASTNode *node) {
      if (node == nullptr || node->type != ASTNodeType::BindingDecl ||
          !samePath(node->location.file, document.path)) {
        return;
      }

      const auto *binding = static_cast<const BindingDeclNode *>(node);
      if (binding->name.empty() || binding->name == "__assign__" ||
          binding->name == "__deref__" || binding->name.front() == '_') {
        return;
      }

      const EnclosingMethodContext context =
          findEnclosingMethodContext(document, binding->location);
      if (context.method == nullptr) {
        return;
      }

      const std::optional<VisibleSymbolInfo> symbol =
          document.analyzer->getResolvedSymbol(binding->location);
      if (!symbol.has_value() || symbol->kind != SymbolKind::Variable) {
        return;
      }

      if (!document.analyzer->getReferenceLocations(binding->location).empty()) {
        return;
      }

      const std::string key = binding->location.file + ":" +
                              std::to_string(binding->location.line) + ":" +
                              std::to_string(binding->location.column);
      if (!emitted.insert(key).second) {
        return;
      }

      diagnostics.push_back(makeUnusedVariableDiagnostic(binding));
    });
  }

  return diagnostics;
}

} // namespace neuron::lsp::detail
