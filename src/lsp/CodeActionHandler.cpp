#include "CodeActionHandler.h"
#include "lsp/LspDocumentQueries.h"
#include "lsp/LspFeaturePayloads.h"
#include "lsp/LspPath.h"
#include "lsp/LspProtocol.h"
#include "lsp/LspText.h"
#include <algorithm>
#include <limits>
#include <sstream>
#include <unordered_set>
namespace neuron::lsp {
namespace {
bool isRenderableType(const NTypePtr &type) {
  return type != nullptr && !type->isUnknown() && !type->isError() &&
         !type->isAuto();
}
std::optional<std::string> extractUndefinedIdentifierName(const std::string &message) {
  constexpr std::string_view prefix = "Undefined identifier:";
  if (message.rfind(prefix, 0) != 0) {
    return std::nullopt;
  }
  std::string name = detail::trimCopy(message.substr(prefix.size()));
  return name.empty() ? std::nullopt : std::optional<std::string>(name);
}

std::optional<std::string> extractTypeMismatchTargetType(const std::string &message) {
  constexpr std::string_view marker = "' to '";
  const std::size_t markerPos = message.find(marker);
  if (markerPos == std::string::npos) {
    return std::nullopt;
  }
  const std::size_t start = markerPos + marker.size();
  const std::size_t end = message.find('\'', start);
  if (end == std::string::npos || end <= start) {
    return std::nullopt;
  }
  return message.substr(start, end - start);
}
std::string_view leadingWhitespace(std::string_view text) {
  std::size_t length = 0;
  while (length < text.size() &&
         (text[length] == ' ' || text[length] == '\t')) {
    ++length;
  }
  return text.substr(0, length);
}
frontend::Range wholeLineRange(const std::string &text, int oneBasedLine) {
  const auto [startOffset, endOffset] = detail::lineOffsetRange(text, oneBasedLine);
  const std::vector<std::size_t> offsets = detail::computeLineOffsets(text);
  const std::size_t lineIndex =
      oneBasedLine > 0 ? static_cast<std::size_t>(oneBasedLine - 1) : 0;

  if (lineIndex + 1 < offsets.size()) {
    return detail::makeFrontendRange(oneBasedLine, 1, oneBasedLine + 1, 1);
  }

  std::size_t trimmedEnd = endOffset;
  while (trimmedEnd > startOffset &&
         (text[trimmedEnd - 1] == '\n' || text[trimmedEnd - 1] == '\r')) {
    --trimmedEnd;
  }
  return detail::makeFrontendRange(
      oneBasedLine, 1, oneBasedLine,
      static_cast<int>(trimmedEnd - startOffset) + 1);
}
std::optional<frontend::Range> bindingTypeAnnotationRange(
    const DocumentState &document, const BindingDeclNode *binding,
    std::string_view expectedType) {
  if (binding == nullptr || binding->typeAnnotation == nullptr) {
    return std::nullopt;
  }

  const std::string_view lineText =
      detail::lineTextView(document.text, binding->location.line);
  if (lineText.empty()) {
    return std::nullopt;
  }

  const std::size_t semicolon = lineText.rfind(';');
  const std::size_t asKeyword =
      semicolon == std::string::npos ? std::string::npos
                                     : lineText.rfind(" as ", semicolon);
  if (semicolon == std::string::npos || asKeyword == std::string::npos ||
      asKeyword >= semicolon) {
    return std::nullopt;
  }

  const std::string annotatedType = detail::trimCopy(
      std::string(lineText.substr(asKeyword + 4, semicolon - (asKeyword + 4))));
  if (!expectedType.empty() && annotatedType != expectedType) {
    return std::nullopt;
  }

  return detail::makeFrontendRange(binding->location.line,
                                   static_cast<int>(asKeyword) + 1,
                                   binding->location.line,
                                   static_cast<int>(semicolon) + 1);
}

std::optional<size_t> findMatchingBraceOffset(const std::string &text,
                                              std::size_t searchStart) {
  const std::size_t openBrace = text.find('{', searchStart);
  if (openBrace == std::string::npos) {
    return std::nullopt;
  }

  bool inString = false;
  bool inLineComment = false;
  bool inBlockComment = false;
  bool escape = false;
  int depth = 0;

  for (std::size_t index = openBrace; index < text.size(); ++index) {
    const char ch = text[index];
    const char next = index + 1 < text.size() ? text[index + 1] : '\0';

    if (inLineComment) {
      if (ch == '\n') {
        inLineComment = false;
      }
      continue;
    }
    if (inBlockComment) {
      if (ch == '*' && next == '/') {
        inBlockComment = false;
        ++index;
      }
      continue;
    }
    if (inString) {
      if (escape) {
        escape = false;
      } else if (ch == '\\') {
        escape = true;
      } else if (ch == '"') {
        inString = false;
      }
      continue;
    }

    if (ch == '/' && next == '/') {
      inLineComment = true;
      ++index;
      continue;
    }
    if (ch == '/' && next == '*') {
      inBlockComment = true;
      ++index;
      continue;
    }
    if (ch == '"') {
      inString = true;
      continue;
    }

    if (ch == '{') {
      ++depth;
    } else if (ch == '}') {
      --depth;
      if (depth == 0) {
        return index;
      }
    }
  }

  return std::nullopt;
}
std::string uniqueParameterName(std::string candidate, std::size_t index,
                                std::unordered_set<std::string> *usedNames) {
  if (!detail::isValidIdentifier(candidate)) {
    candidate = "arg" + std::to_string(index + 1);
  }

  if (usedNames == nullptr) {
    return candidate;
  }

  std::string unique = candidate;
  std::size_t suffix = 2;
  while (!usedNames->insert(unique).second) {
    unique = candidate + std::to_string(suffix++);
  }
  return unique;
}

std::string buildStubMethodText(const CallExprNode *call,
                                const SemanticAnalyzer *analyzer,
                                const std::string &methodName,
                                const std::string &indent,
                                const std::string &lineEnding) {
  std::ostringstream out;
  out << indent << methodName << " method(";

  std::unordered_set<std::string> usedNames;
  if (call != nullptr) {
    for (std::size_t index = 0; index < call->arguments.size(); ++index) {
      if (index > 0) {
        out << ", ";
      }

      std::string candidate;
      if (index < call->argumentLabels.size() &&
          !call->argumentLabels[index].empty()) {
        candidate = call->argumentLabels[index];
      } else if (call->arguments[index] != nullptr &&
                 call->arguments[index]->type == ASTNodeType::Identifier) {
        candidate =
            static_cast<const IdentifierNode *>(call->arguments[index].get())->name;
      }

      const std::string parameterName =
          uniqueParameterName(std::move(candidate), index, &usedNames);
      std::string parameterType = "dynamic";
      if (analyzer != nullptr && call->arguments[index] != nullptr) {
        const NTypePtr inferred = analyzer->getInferredType(call->arguments[index].get());
        if (isRenderableType(inferred)) {
          parameterType = inferred->toString();
        }
      }

      out << parameterName << " as " << parameterType;
    }
  }

  out << ") as dynamic {" << lineEnding;
  out << indent << "    return null;" << lineEnding;
  out << indent << "}";
  return out.str();
}

} // namespace

CodeActionHandler::CodeActionHandler(LspDispatcher &dispatcher,
                                     DocumentManager &documents,
                                     LspTransport &transport)
    : m_documents(documents), m_transport(transport) {
  dispatcher.registerHandler("textDocument/codeAction",
                             [this](const LspMessageContext &context) {
                               return handleCodeAction(context);
                             });
}

DispatchResult CodeActionHandler::handleCodeAction(
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
  if (document == nullptr) {
    m_transport.sendResult(*context.id, llvm::json::Array{});
    return DispatchResult::Continue;
  }

  frontend::Range requestedRange =
      detail::makeFrontendRange(1, 1, std::numeric_limits<int>::max(),
                                std::numeric_limits<int>::max());
  if (const auto *rangeObject = context.params->getObject("range")) {
    requestedRange = detail::parseLspRange(rangeObject);
  }

  std::vector<frontend::Diagnostic> diagnostics;
  if (const auto *requestContext = context.params->getObject("context")) {
    if (const auto *items = requestContext->getArray("diagnostics")) {
      for (const llvm::json::Value &entryValue : *items) {
        const auto *entry = entryValue.getAsObject();
        if (entry == nullptr) {
          continue;
        }
        frontend::Diagnostic diagnostic;
        diagnostic.file = document->path.string();
        diagnostic.phase = "semantic";
        diagnostic.range = detail::parseLspRange(entry->getObject("range"));
        diagnostic.message = entry->getString("message").value_or("").str();
        diagnostic.code = entry->getString("code").value_or("").str();
        diagnostics.push_back(std::move(diagnostic));
      }
    }
  }
  if (diagnostics.empty()) {
    diagnostics = document->diagnostics;
  }

  llvm::json::Array actions;
  std::unordered_set<std::string> emittedTitles;
  const std::string lineEnding = detail::detectLineEnding(document->text);
  const std::string documentUri = std::string(uri->str());
  const auto appendQuickFix = [&](std::string title, llvm::json::Array edits,
                                  const frontend::Diagnostic &diagnostic,
                                  bool preferred) {
    if (!emittedTitles.insert(title).second) {
      return;
    }

    llvm::json::Object action;
    action["title"] = std::move(title);
    action["kind"] = "quickfix";
    action["isPreferred"] = preferred;
    action["edit"] = detail::makeWorkspaceEditForUri(documentUri, std::move(edits));
    action["diagnostics"] = llvm::json::Array{detail::makeLspDiagnostic(diagnostic)};
    actions.push_back(std::move(action));
  };

  for (const frontend::Diagnostic &diagnostic : diagnostics) {
    if (!detail::rangeOverlaps(diagnostic.range, requestedRange)) {
      continue;
    }

    const SourceLocation location = {diagnostic.range.start.line,
                                     diagnostic.range.start.column,
                                     document->path.string()};

    if (diagnostic.code == kUnusedVariableDiagnosticCode ||
        diagnostic.message.rfind(kUnusedVariableDiagnosticPrefix, 0) == 0) {
      if (const auto *binding = detail::findBindingInDocument(*document, location)) {
        llvm::json::Array edits;
        edits.push_back(detail::makeLspTextEdit(
            wholeLineRange(document->text, binding->location.line), ""));
        appendQuickFix("Delete unused variable '" + binding->name + "'",
                       std::move(edits), diagnostic, true);
      }
      continue;
    }

    if (const std::optional<std::string> targetType =
            extractTypeMismatchTargetType(diagnostic.message)) {
      const auto *binding = detail::findBindingInDocument(*document, location);
      if (binding != nullptr && binding->value != nullptr &&
          binding->typeAnnotation != nullptr && binding->name != "__assign__" &&
          binding->name != "__deref__") {
        const std::optional<frontend::Range> annotationRange =
            bindingTypeAnnotationRange(*document, binding, *targetType);
        if (annotationRange.has_value()) {
          const std::string_view lineText =
              detail::lineTextView(document->text, binding->location.line);
          const std::string_view indentView = leadingWhitespace(lineText);
          const std::string indent(indentView.begin(), indentView.end());

          llvm::json::Array edits;
          edits.push_back(detail::makeLspTextEdit(*annotationRange, ""));
          edits.push_back(detail::makeLspTextEdit(
              detail::makeFrontendRange(binding->location.line,
                                        static_cast<int>(lineText.size()) + 1,
                                        binding->location.line,
                                        static_cast<int>(lineText.size()) + 1),
              lineEnding + indent + binding->name + " as " + *targetType + ";"));
          appendQuickFix("Insert cast to '" + *targetType + "'",
                         std::move(edits), diagnostic, true);
        }
      }
    }

    if (const std::optional<std::string> missingName =
            extractUndefinedIdentifierName(diagnostic.message)) {
      if (!detail::isValidIdentifier(*missingName)) {
        continue;
      }
      EnclosingMethodContext methodContext;
      const CallExprNode *call =
          detail::findCallInDocument(*document, location, &methodContext);
      if (call == nullptr || call->callee == nullptr ||
          call->callee->type != ASTNodeType::Identifier ||
          static_cast<const IdentifierNode *>(call->callee.get())->name !=
              *missingName) {
        continue;
      }

      frontend::Position insertPosition =
          detail::offsetToPosition(document->text, document->text.size());
      std::string insertText;
      std::string title = "Create function stub '" + *missingName + "'";

      if (methodContext.ownerClass != nullptr) {
        const std::size_t classOffset =
            detail::positionToOffset(document->text,
                                     methodContext.ownerClass->location.line - 1,
                                     methodContext.ownerClass->location.column - 1);
        if (const std::optional<size_t> closeBrace =
                findMatchingBraceOffset(document->text, classOffset)) {
          std::string indent;
          if (!methodContext.ownerClass->members.empty()) {
            const std::string_view memberIndent = leadingWhitespace(
                detail::lineTextView(
                    document->text,
                    methodContext.ownerClass->members.front()->location.line));
            indent.assign(memberIndent.begin(), memberIndent.end());
          } else {
            const std::string_view classIndent = leadingWhitespace(
                detail::lineTextView(document->text,
                                     methodContext.ownerClass->location.line));
            indent.assign(classIndent.begin(), classIndent.end());
            indent += "    ";
          }

          insertPosition = detail::offsetToPosition(document->text, *closeBrace);
          insertText = buildStubMethodText(call, document->analyzer.get(), *missingName,
                                           indent, lineEnding) +
                       lineEnding;
          if (*closeBrace > 0 && document->text[*closeBrace - 1] != '\n' &&
              document->text[*closeBrace - 1] != '\r') {
            insertText = lineEnding + insertText;
          }
          title = "Create method stub '" + *missingName + "'";
        }
      }

      if (insertText.empty()) {
        insertText = buildStubMethodText(call, document->analyzer.get(),
                                         *missingName, "", lineEnding) +
                     lineEnding;
        if (!document->text.empty()) {
          const bool endsWithNewline =
              document->text.back() == '\n' || document->text.back() == '\r';
          insertText = (endsWithNewline ? lineEnding : lineEnding + lineEnding) +
                       insertText;
        }
      }

      llvm::json::Array edits;
      edits.push_back(detail::makeLspTextEdit(
          detail::makeFrontendRange(insertPosition.line, insertPosition.column,
                                    insertPosition.line, insertPosition.column),
          insertText));
      appendQuickFix(std::move(title), std::move(edits), diagnostic, true);
    }
  }

  m_transport.sendResult(*context.id, std::move(actions));
  return DispatchResult::Continue;
}

} // namespace neuron::lsp
