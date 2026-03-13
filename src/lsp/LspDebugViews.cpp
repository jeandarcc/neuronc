#include "LspDebugViews.h"

#include "LspWorkspace.h"

#include "neuronc/cli/SettingsMacros.h"
#include "neuronc/frontend/Diagnostics.h"
#include "neuronc/frontend/Frontend.h"
#include "neuronc/lexer/Lexer.h"
#include "neuronc/mir/MIRBuilder.h"
#include "neuronc/mir/MIROwnershipPass.h"
#include "neuronc/mir/MIRPrinter.h"
#include "neuronc/nir/NIRBuilder.h"
#include "neuronc/nir/Optimizer.h"

#include <algorithm>
#include <sstream>

namespace neuron::lsp::detail {

namespace {

std::vector<Token> lexSource(const std::string &text, const fs::path &path,
                             std::vector<std::string> *outErrors) {
  Lexer lexer(text, path.string());
  std::vector<Token> tokens = lexer.tokenize();
  if (outErrors != nullptr) {
    *outErrors = lexer.errors();
  }
  return tokens;
}

bool isTightLeft(TokenType type) {
  switch (type) {
  case TokenType::LeftParen:
  case TokenType::LeftBracket:
  case TokenType::RightParen:
  case TokenType::RightBracket:
  case TokenType::RightBrace:
  case TokenType::Comma:
  case TokenType::Semicolon:
  case TokenType::Colon:
  case TokenType::Dot:
  case TokenType::DotDot:
    return true;
  default:
    return false;
  }
}

bool isTightRight(TokenType type) {
  switch (type) {
  case TokenType::LeftParen:
  case TokenType::LeftBracket:
  case TokenType::LeftBrace:
  case TokenType::Dot:
  case TokenType::DotDot:
    return true;
  default:
    return false;
  }
}

std::string formatDiagnostic(const frontend::Diagnostic &diagnostic) {
  std::ostringstream out;
  out << diagnostic.file << ":" << diagnostic.range.start.line << ":"
      << diagnostic.range.start.column;
  if (!diagnostic.code.empty()) {
    out << " [" << diagnostic.code << "]";
  }
  out << " " << diagnostic.message;
  return out.str();
}

void appendCommentLine(std::string *out, const std::string &line) {
  if (out == nullptr) {
    return;
  }
  out->append("// ");
  out->append(line);
  out->push_back('\n');
}

void appendDiagnosticsAsComments(
    std::string *out, std::string_view title,
    const std::vector<frontend::Diagnostic> &diagnostics) {
  if (out == nullptr || diagnostics.empty()) {
    return;
  }
  appendCommentLine(out, std::string(title));
  for (const auto &diagnostic : diagnostics) {
    appendCommentLine(out, formatDiagnostic(diagnostic));
  }
  out->push_back('\n');
}

std::vector<Token> debugTokensFor(const DocumentState &document) {
  if (!document.expandedTokens.empty()) {
    return document.expandedTokens;
  }

  std::vector<std::string> lexerErrors;
  return lexSource(document.text, document.path, &lexerErrors);
}

std::string buildExpandedSourceContent(const DocumentState &document) {
  std::string content;
  appendDiagnosticsAsComments(&content, "Macro expansion diagnostics:",
                              document.configDiagnostics);
  content += renderTokensAsSource(debugTokensFor(document));
  return content;
}

DebugViewContent buildMirContent(const DocumentState &document,
                                 const fs::path &workspaceRoot) {
  DebugViewContent view;
  view.title = document.path.filename().string() + " [MIR]";
  view.languageId = "plaintext";

  std::vector<Token> tokens = debugTokensFor(document);
  const std::string expandedSource = renderTokensAsSource(tokens);

  std::vector<frontend::Diagnostic> diagnostics = document.configDiagnostics;
  frontend::ParseResult parseResult =
      frontend::parseTokens(std::move(tokens), document.path.string());
  diagnostics.insert(diagnostics.end(), parseResult.diagnostics.begin(),
                     parseResult.diagnostics.end());
  if (parseResult.program == nullptr || parseResult.hasErrors()) {
    appendDiagnosticsAsComments(&view.content, "MIR unavailable. Parse failed:",
                                diagnostics);
    view.content += expandedSource;
    return view;
  }

  frontend::SemanticOptions options = makeSemanticOptions(document, workspaceRoot);
  options.sourceText = expandedSource;
  frontend::SemanticResult semanticResult =
      frontend::analyzeProgram(parseResult.program.get(), options);
  diagnostics.insert(diagnostics.end(), semanticResult.diagnostics.begin(),
                     semanticResult.diagnostics.end());
  appendDiagnosticsAsComments(&view.content, "Diagnostics:", diagnostics);

  mir::MIRBuilder builder;
  auto module =
      builder.build(parseResult.program.get(), document.path.stem().string());
  if (module == nullptr) {
    view.content += "MIR generation failed.\n";
    return view;
  }

  if (builder.hasErrors()) {
    const auto builderDiagnostics = frontend::convertStringDiagnostics(
        "mir", document.path.string(), builder.errors());
    appendDiagnosticsAsComments(&view.content, "MIR builder diagnostics:",
                                builderDiagnostics);
  }

  mir::MIROwnershipPass ownershipPass;
  ownershipPass.setSourceText(expandedSource);
  ownershipPass.run(*module);
  appendDiagnosticsAsComments(&view.content, "Ownership diagnostics:",
                              frontend::convertSemanticDiagnostics(
                                  ownershipPass.errors()));

  view.content += mir::printToString(*module);
  return view;
}

DebugViewContent buildNirContent(const DocumentState &document,
                                 const fs::path &workspaceRoot) {
  DebugViewContent view;
  view.title = document.path.filename().string() + " [NIR]";
  view.languageId = "plaintext";

  std::vector<Token> tokens = debugTokensFor(document);
  const std::string expandedSource = renderTokensAsSource(tokens);

  std::vector<frontend::Diagnostic> diagnostics = document.configDiagnostics;
  frontend::ParseResult parseResult =
      frontend::parseTokens(std::move(tokens), document.path.string());
  diagnostics.insert(diagnostics.end(), parseResult.diagnostics.begin(),
                     parseResult.diagnostics.end());
  if (parseResult.program == nullptr || parseResult.hasErrors()) {
    appendDiagnosticsAsComments(&view.content, "NIR unavailable. Parse failed:",
                                diagnostics);
    view.content += expandedSource;
    return view;
  }

  frontend::SemanticOptions options = makeSemanticOptions(document, workspaceRoot);
  options.sourceText = expandedSource;
  frontend::SemanticResult semanticResult =
      frontend::analyzeProgram(parseResult.program.get(), options);
  diagnostics.insert(diagnostics.end(), semanticResult.diagnostics.begin(),
                     semanticResult.diagnostics.end());
  if (semanticResult.analyzer == nullptr || !semanticResult.diagnostics.empty()) {
    appendDiagnosticsAsComments(
        &view.content, "NIR unavailable. Semantic analysis failed:", diagnostics);
    view.content += expandedSource;
    return view;
  }

  nir::NIRBuilder builder;
  auto module =
      builder.build(parseResult.program.get(), document.path.stem().string());
  if (builder.hasErrors() || module == nullptr) {
    const auto builderDiagnostics = frontend::convertStringDiagnostics(
        "semantic", document.path.string(), builder.errors());
    diagnostics.insert(diagnostics.end(), builderDiagnostics.begin(),
                       builderDiagnostics.end());
    appendDiagnosticsAsComments(&view.content, "NIR generation failed:",
                                diagnostics);
    view.content += expandedSource;
    return view;
  }

  view.content += "=== Unoptimized NIR ===\n";
  view.content += module->toString();

  auto optimizer = nir::Optimizer::createDefaultOptimizer();
  optimizer->run(module.get());

  view.content += "\n=== Optimized NIR ===\n";
  view.content += module->toString();
  return view;
}

} // namespace

void refreshMacroExpansionState(DocumentState &document, const fs::path &toolRoot) {
  document.expandedTokens.clear();
  document.macroExpansions.clear();
  document.configDiagnostics.clear();

  std::vector<std::string> lexerErrors;
  std::vector<Token> sourceTokens = lexSource(document.text, document.path, &lexerErrors);
  if (!lexerErrors.empty()) {
    document.expandedTokens = std::move(sourceTokens);
    return;
  }

  cli::SettingsMacroProcessor processor(toolRoot, document.path);
  std::vector<std::string> configErrors;
  if (!processor.initialize(&configErrors)) {
    document.expandedTokens = std::move(sourceTokens);
    document.configDiagnostics = frontend::convertStringDiagnostics(
        "config", document.path.string(), configErrors);
    return;
  }

  std::vector<cli::MacroExpansionTrace> traces;
  if (!processor.expandSourceTokensWithTrace(document.path, sourceTokens,
                                             &document.expandedTokens, &traces,
                                             &configErrors)) {
    document.expandedTokens = std::move(sourceTokens);
    document.configDiagnostics = frontend::convertStringDiagnostics(
        "config", document.path.string(), configErrors);
    return;
  }

  document.configDiagnostics = frontend::convertStringDiagnostics(
      "config", document.path.string(), configErrors);
  document.macroExpansions.reserve(traces.size());
  for (const auto &trace : traces) {
    document.macroExpansions.push_back(
        MacroExpansionRecord{trace.useLocation, trace.useLength, trace.name,
                             trace.qualifiedName, trace.expansion});
  }
  std::sort(document.macroExpansions.begin(), document.macroExpansions.end(),
            [](const MacroExpansionRecord &lhs, const MacroExpansionRecord &rhs) {
              if (lhs.location.line != rhs.location.line) {
                return lhs.location.line < rhs.location.line;
              }
              if (lhs.location.column != rhs.location.column) {
                return lhs.location.column < rhs.location.column;
              }
              return lhs.qualifiedName < rhs.qualifiedName;
            });
}

std::string renderTokensAsSource(const std::vector<Token> &tokens) {
  std::string rendered;
  int indent = 0;
  bool lineStart = true;
  bool havePrevious = false;
  TokenType previousType = TokenType::Eof;

  const auto appendIndent = [&]() {
    if (!lineStart) {
      return;
    }
    rendered.append(static_cast<std::size_t>(std::max(0, indent)) * 2, ' ');
    lineStart = false;
  };

  for (const Token &token : tokens) {
    if (token.type == TokenType::Eof || token.value.empty()) {
      continue;
    }

    if (token.type == TokenType::RightBrace) {
      if (!rendered.empty() && rendered.back() != '\n') {
        rendered.push_back('\n');
      }
      indent = std::max(0, indent - 1);
      lineStart = true;
    }

    const bool beganLine = lineStart;
    appendIndent();

    if (havePrevious && !beganLine && !isTightRight(previousType) &&
        !isTightLeft(token.type)) {
      rendered.push_back(' ');
    }

    rendered += token.value;
    havePrevious = true;
    previousType = token.type;

    switch (token.type) {
    case TokenType::LeftBrace:
      ++indent;
      rendered.push_back('\n');
      lineStart = true;
      break;
    case TokenType::Semicolon:
      rendered.push_back('\n');
      lineStart = true;
      break;
    case TokenType::Comma:
      rendered.push_back(' ');
      break;
    case TokenType::RightBrace:
      rendered.push_back('\n');
      lineStart = true;
      break;
    default:
      break;
    }
  }

  while (!rendered.empty() && (rendered.back() == ' ' || rendered.back() == '\n')) {
    rendered.pop_back();
  }
  if (!rendered.empty()) {
    rendered.push_back('\n');
  }
  return rendered;
}

DebugViewContent buildDebugView(const DocumentState &document,
                                const fs::path &workspaceRoot,
                                const fs::path &toolRoot,
                                std::string_view viewKind) {
  (void)toolRoot;
  if (viewKind == "expanded") {
    DebugViewContent view;
    view.title = document.path.filename().string() + " [Expanded Source]";
    // Use canonical language id 'neuron' for editor debug views
    view.languageId = "neuron";
    view.content = buildExpandedSourceContent(document);
    return view;
  }
  if (viewKind == "nir") {
    return buildNirContent(document, workspaceRoot);
  }
  if (viewKind == "mir") {
    return buildMirContent(document, workspaceRoot);
  }

  DebugViewContent view;
  view.title = document.path.filename().string() + " [Debug View]";
  view.languageId = "plaintext";
  view.content = "Unknown debug view kind: " + std::string(viewKind) + "\n";
  return view;
}

} // namespace neuron::lsp::detail
