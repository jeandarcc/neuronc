#pragma once

#include "llvm/Support/JSON.h"
#include "neuronc/frontend/Frontend.h"
#include "neuronc/parser/AST.h"

#include <array>
#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace neuron::lsp {

namespace fs = std::filesystem;

struct ChunkSpan {
  std::size_t startOffset = 0;
  std::size_t endOffset = 0;
  int startLine = 1;
  int startColumn = 1;
};

struct TokenMatch {
  const Token *token = nullptr;
  const ASTNode *memberAccess = nullptr;
};

struct DocumentChunk {
  ChunkSpan span;
  std::string text;
  frontend::ParseResult parseResult;
};

struct MacroExpansionRecord {
  SourceLocation location;
  int length = 1;
  std::string name;
  std::string qualifiedName;
  std::string expansion;
};

struct OwnershipHintRecord {
  SourceLocation location;
  int length = 1;
  std::string label;
  std::string tooltip;
};

struct DocumentState {
  std::string uri;
  fs::path path;
  std::string text;
  int version = 0;
  std::vector<DocumentChunk> chunks;
  std::vector<Token> expandedTokens;
  std::vector<MacroExpansionRecord> macroExpansions;
  std::vector<OwnershipHintRecord> ownershipHints;
  std::vector<frontend::Diagnostic> configDiagnostics;
  std::vector<frontend::Diagnostic> diagnostics;
  std::unique_ptr<SemanticAnalyzer> analyzer;
  std::vector<ASTNode *> declarations;
};

struct DocumentChange {
  std::optional<frontend::Range> range;
  std::string text;
};

struct WorkspaceModuleIndexEntry {
  fs::path path;
  bool projectLocal = false;
};

struct ResolvedWorkspaceSource {
  std::string moduleName;
  fs::path path;
  std::string text;
};

struct WorkspaceSemanticState {
  fs::path entryPath;
  std::vector<Token> entryTokens;
  std::unique_ptr<ProgramNode> mergedProgram;
  frontend::SemanticResult semanticResult;
};

struct SemanticTokenEntry {
  int line = 0;
  int character = 0;
  int length = 0;
  int tokenType = 0;
  int tokenModifiers = 0;
};

struct WorkspaceSymbolEntry {
  std::string name;
  SymbolKind kind = SymbolKind::Variable;
  SymbolRange range;
  std::string containerName;
};

struct WorkspaceTypeRecord {
  std::string name;
  ClassKind kind = ClassKind::Class;
  fs::path path;
  SourceLocation location;
  std::vector<std::string> baseClasses;
};

struct EnclosingMethodContext {
  const ProgramNode *program = nullptr;
  const MethodDeclNode *method = nullptr;
  const ClassDeclNode *ownerClass = nullptr;
};

enum class LspSemanticTokenType {
  Namespace = 0,
  Type = 1,
  Function = 2,
  Parameter = 3,
  Variable = 4,
  Property = 5,
};

enum class DispatchResult {
  Continue,
  Stop,
};

using WorkspaceFileReader =
    std::function<std::optional<std::string>(const fs::path &)>;

inline constexpr std::array<const char *, 6> kSemanticTokenLegend = {
    "namespace", "type", "function", "parameter", "variable", "property"};
inline constexpr std::string_view kUnusedVariableDiagnosticCode = "NR9002";
inline constexpr std::string_view kUnusedVariableDiagnosticPrefix =
    "Unused variable: ";
inline constexpr std::size_t kMaxWorkspaceSymbolResults = 512;

} // namespace neuron::lsp
