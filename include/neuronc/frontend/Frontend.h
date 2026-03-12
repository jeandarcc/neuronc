#pragma once

#include "neuronc/frontend/Diagnostics.h"
#include "neuronc/lexer/Token.h"
#include "neuronc/parser/AST.h"
#include "neuronc/sema/SemanticAnalyzer.h"

#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace neuron::frontend {

struct ParseResult {
  std::vector<Token> tokens;
  std::unique_ptr<ProgramNode> program;
  std::vector<std::string> lexerErrors;
  std::vector<std::string> parserErrors;
  std::vector<Diagnostic> diagnostics;

  bool hasErrors() const {
    return !lexerErrors.empty() || !parserErrors.empty();
  }
};

struct SemanticOptions {
  std::unordered_set<std::string> availableModules;
  bool enforceModuleResolution = false;
  int maxClassesPerFile = 0;
  bool requireMethodUppercaseStart = false;
  bool enforceStrictFileNaming = false;
  std::string sourceFileStem;
  int maxLinesPerMethod = 0;
  int maxLinesPerBlockStatement = 0;
  int minMethodNameLength = 0;
  bool requireClassExplicitVisibility = false;
  bool requirePropertyExplicitVisibility = false;
  bool requireConstUppercase = false;
  int maxNestingDepth = 0;
  bool requirePublicMethodDocs = false;
  bool agentHints = false;
  std::string sourceText;
  std::unordered_map<std::string, NativeModuleInfo> moduleCppModules;
};

struct SemanticResult {
  std::unique_ptr<SemanticAnalyzer> analyzer;
  std::vector<Diagnostic> diagnostics;

  bool hasErrors() const { return !diagnostics.empty(); }
};

struct Location {
  std::string file;
  Range range;
};

struct DocumentSymbol {
  std::string name;
  SymbolKind kind = SymbolKind::Variable;
  Range range;
  Range selectionRange;
  std::vector<DocumentSymbol> children;
};

ParseResult lexAndParseSource(const std::string &source,
                              const std::string &filename);

ParseResult parseTokens(std::vector<Token> tokens, const std::string &filename);

void applySemanticOptions(SemanticAnalyzer *sema,
                          const SemanticOptions &options);

SemanticResult analyzeProgram(ProgramNode *program,
                              const SemanticOptions &options);

SemanticResult analyzeProgramView(const ProgramView &program,
                                  const SemanticOptions &options);

std::optional<Location> getDefinition(const std::string &file, int line,
                                      int col);

std::vector<Location> getReferences(const std::string &file, int line, int col);

std::vector<DocumentSymbol> getDocumentSymbols(const std::string &file);

std::optional<Location> getDefinition(const std::string &source,
                                      const std::string &filename, int line,
                                      int col,
                                      const SemanticOptions &options);

std::vector<Location> getReferences(const std::string &source,
                                    const std::string &filename, int line,
                                    int col,
                                    const SemanticOptions &options);

std::vector<DocumentSymbol> getDocumentSymbols(const std::string &source,
                                               const std::string &filename,
                                               const SemanticOptions &options);

} // namespace neuron::frontend
