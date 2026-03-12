#include "neuronc/frontend/Frontend.h"

#include "neuronc/lexer/Lexer.h"
#include "neuronc/parser/Parser.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iterator>

namespace neuron::frontend {

namespace {

namespace fs = std::filesystem;

struct NavigationState {
  ParseResult parseResult;
  SemanticResult semanticResult;
};

Range makeRange(const neuron::SymbolLocation &location) {
  Range range;
  range.start.line = location.location.line;
  range.start.column = location.location.column;
  range.end.line = location.location.line;
  range.end.column = location.location.column + std::max(1, location.length);
  return range;
}

Location makeLocation(const neuron::SymbolLocation &location) {
  Location result;
  result.file = location.location.file;
  result.range = makeRange(location);
  return result;
}

DocumentSymbol convertDocumentSymbol(const neuron::DocumentSymbolInfo &symbol) {
  DocumentSymbol result;
  result.name = symbol.name;
  result.kind = symbol.kind;
  result.range.start.line = symbol.range.start.line;
  result.range.start.column = symbol.range.start.column;
  result.range.end.line = symbol.range.end.line;
  result.range.end.column = symbol.range.end.column;
  result.selectionRange.start.line = symbol.selectionRange.start.line;
  result.selectionRange.start.column = symbol.selectionRange.start.column;
  result.selectionRange.end.line = symbol.selectionRange.end.line;
  result.selectionRange.end.column = symbol.selectionRange.end.column;
  for (const auto &child : symbol.children) {
    result.children.push_back(convertDocumentSymbol(child));
  }
  return result;
}

const Token *findTokenAt(const std::vector<Token> &tokens, int line, int col) {
  for (const auto &token : tokens) {
    if (token.type == TokenType::Eof || token.location.line != line) {
      continue;
    }
    const int length =
        std::max(1, static_cast<int>(token.value.empty() ? 1 : token.value.size()));
    if (col >= token.location.column && col < token.location.column + length) {
      return &token;
    }
  }
  return nullptr;
}

SemanticOptions buildQueryOptions(const SemanticOptions &options,
                                  const std::string &source,
                                  const std::string &filename) {
  SemanticOptions effective = options;
  if (effective.sourceText.empty()) {
    effective.sourceText = source;
  }
  if (effective.sourceFileStem.empty()) {
    effective.sourceFileStem = fs::path(filename).stem().string();
  }
  return effective;
}

std::optional<NavigationState>
analyzeNavigationSource(const std::string &source, const std::string &filename,
                        const SemanticOptions &options) {
  NavigationState state;
  state.parseResult = lexAndParseSource(source, filename);
  if (state.parseResult.program == nullptr) {
    return std::nullopt;
  }
  state.semanticResult =
      analyzeProgram(state.parseResult.program.get(),
                     buildQueryOptions(options, source, filename));
  if (state.semanticResult.analyzer == nullptr) {
    return std::nullopt;
  }
  return state;
}

std::optional<std::string> readFile(const std::string &filename) {
  std::ifstream input(filename, std::ios::binary);
  if (!input) {
    return std::nullopt;
  }
  return std::string(std::istreambuf_iterator<char>(input),
                     std::istreambuf_iterator<char>());
}

} // namespace

ParseResult lexAndParseSource(const std::string &source,
                              const std::string &filename) {
  ParseResult result;
  neuron::Lexer lexer(source, filename);
  result.tokens = lexer.tokenize();
  result.lexerErrors = lexer.errors();
  result.diagnostics =
      convertStringDiagnostics("lexer", filename, result.lexerErrors);
  if (!result.lexerErrors.empty()) {
    return result;
  }
  return parseTokens(std::move(result.tokens), filename);
}

ParseResult parseTokens(std::vector<Token> tokens, const std::string &filename) {
  ParseResult result;
  result.tokens = std::move(tokens);
  neuron::Parser parser(result.tokens, filename);
  result.program = parser.parse();
  result.parserErrors = parser.errors();
  result.diagnostics = convertParserDiagnostics(filename, parser.diagnostics());
  return result;
}

void applySemanticOptions(SemanticAnalyzer *sema,
                          const SemanticOptions &options) {
  if (sema == nullptr) {
    return;
  }
  sema->setAvailableModules(options.availableModules,
                            options.enforceModuleResolution);
  sema->setMaxClassesPerFile(options.maxClassesPerFile);
  sema->setRequireMethodUppercaseStart(options.requireMethodUppercaseStart);
  sema->setStrictFileNamingRules(options.enforceStrictFileNaming,
                                 options.sourceFileStem);
  sema->setMaxLinesPerMethod(options.maxLinesPerMethod);
  sema->setMaxLinesPerBlockStatement(options.maxLinesPerBlockStatement);
  sema->setMinMethodNameLength(options.minMethodNameLength);
  sema->setRequireClassExplicitVisibility(
      options.requireClassExplicitVisibility);
  sema->setRequirePropertyExplicitVisibility(
      options.requirePropertyExplicitVisibility);
  sema->setRequireConstUppercase(options.requireConstUppercase);
  sema->setMaxNestingDepth(options.maxNestingDepth);
  sema->setRequirePublicMethodDocs(options.requirePublicMethodDocs);
  sema->setSourceText(options.sourceText);
  sema->setAgentHints(options.agentHints);
  sema->setModuleCppModules(options.moduleCppModules);
}

SemanticResult analyzeProgram(ProgramNode *program,
                              const SemanticOptions &options) {
  SemanticResult result;
  result.analyzer = std::make_unique<SemanticAnalyzer>();
  applySemanticOptions(result.analyzer.get(), options);
  result.analyzer->analyze(program);
  result.diagnostics =
      convertSemanticDiagnostics(result.analyzer->getErrors());
  return result;
}

SemanticResult analyzeProgramView(const ProgramView &program,
                                  const SemanticOptions &options) {
  SemanticResult result;
  result.analyzer = std::make_unique<SemanticAnalyzer>();
  applySemanticOptions(result.analyzer.get(), options);
  result.analyzer->analyze(program);
  result.diagnostics =
      convertSemanticDiagnostics(result.analyzer->getErrors());
  return result;
}

std::optional<Location> getDefinition(const std::string &file, int line,
                                      int col) {
  const std::optional<std::string> source = readFile(file);
  if (!source.has_value()) {
    return std::nullopt;
  }
  SemanticOptions options;
  return getDefinition(*source, file, line, col, options);
}

std::vector<Location> getReferences(const std::string &file, int line, int col) {
  const std::optional<std::string> source = readFile(file);
  if (!source.has_value()) {
    return {};
  }
  SemanticOptions options;
  return getReferences(*source, file, line, col, options);
}

std::vector<DocumentSymbol> getDocumentSymbols(const std::string &file) {
  const std::optional<std::string> source = readFile(file);
  if (!source.has_value()) {
    return {};
  }
  SemanticOptions options;
  return getDocumentSymbols(*source, file, options);
}

std::optional<Location> getDefinition(const std::string &source,
                                      const std::string &filename, int line,
                                      int col,
                                      const SemanticOptions &options) {
  const std::optional<NavigationState> state =
      analyzeNavigationSource(source, filename, options);
  if (!state.has_value()) {
    return std::nullopt;
  }

  const Token *token = findTokenAt(state->parseResult.tokens, line, col);
  if (token == nullptr || token->type != TokenType::Identifier) {
    return std::nullopt;
  }

  const std::optional<neuron::SymbolLocation> definition =
      state->semanticResult.analyzer->getDefinitionLocation(token->location);
  if (!definition.has_value()) {
    return std::nullopt;
  }
  return makeLocation(*definition);
}

std::vector<Location> getReferences(const std::string &source,
                                    const std::string &filename, int line,
                                    int col,
                                    const SemanticOptions &options) {
  const std::optional<NavigationState> state =
      analyzeNavigationSource(source, filename, options);
  if (!state.has_value()) {
    return {};
  }

  const Token *token = findTokenAt(state->parseResult.tokens, line, col);
  if (token == nullptr || token->type != TokenType::Identifier) {
    return {};
  }

  std::vector<Location> locations;
  for (const auto &reference :
       state->semanticResult.analyzer->getReferenceLocations(token->location)) {
    locations.push_back(makeLocation(reference));
  }
  return locations;
}

std::vector<DocumentSymbol> getDocumentSymbols(const std::string &source,
                                               const std::string &filename,
                                               const SemanticOptions &options) {
  const std::optional<NavigationState> state =
      analyzeNavigationSource(source, filename, options);
  if (!state.has_value()) {
    return {};
  }

  std::vector<DocumentSymbol> symbols;
  for (const auto &symbol : state->semanticResult.analyzer->getDocumentSymbols()) {
    symbols.push_back(convertDocumentSymbol(symbol));
  }
  return symbols;
}

} // namespace neuron::frontend
