#include "neuronc/cli/commands/DebugCommands.h"

#include "neuronc/cli/SettingsMacros.h"
#include "neuronc/lexer/Lexer.h"
#include "neuronc/nir/NIRBuilder.h"
#include "neuronc/nir/Optimizer.h"
#include "neuronc/parser/Parser.h"

#include <iostream>

namespace neuron::cli {

namespace {

bool expandDebugSource(const std::filesystem::path &filepath,
                       const std::filesystem::path &toolRoot,
                       const std::string &source,
                       bool *outIsConfigError,
                       std::vector<std::string> *outErrors,
                       std::vector<neuron::Token> *outTokens) {
  if (outIsConfigError != nullptr) {
    *outIsConfigError = false;
  }
  neuron::Lexer lexer(source, filepath.string());
  auto tokens = lexer.tokenize();
  if (!lexer.errors().empty()) {
    if (outErrors != nullptr) {
      *outErrors = lexer.errors();
    }
    return false;
  }

  neuron::cli::SettingsMacroProcessor macroProcessor(toolRoot, filepath);
  if (!macroProcessor.initialize(outErrors)) {
    if (outIsConfigError != nullptr) {
      *outIsConfigError = true;
    }
    return false;
  }
  const bool ok =
      macroProcessor.expandSourceTokens(filepath, tokens, outTokens, outErrors);
  if (!ok && outIsConfigError != nullptr) {
    *outIsConfigError = true;
  }
  return ok;
}

} // namespace

int runLexCommand(const std::string &filepath,
                  const DebugCommandDependencies &deps) {
  const NeuronSettings settings = deps.loadNeuronSettings(std::filesystem::path(filepath));
  std::string source = deps.readFile(filepath, settings);
  if (source.empty()) {
    return 1;
  }

  std::vector<std::string> errors;
  std::vector<neuron::Token> tokens;
  bool configError = false;
  if (!expandDebugSource(std::filesystem::path(filepath), deps.toolRoot, source,
                         &configError, &errors, &tokens)) {
    deps.reportStringDiagnostics(configError ? "config" : "lexer", filepath, source,
                                 errors);
    return 1;
  }

  std::cout << "=== Tokens (" << filepath << ") ===" << std::endl;
  for (const auto &token : tokens) {
    std::cout << "  " << token << std::endl;
  }

  std::cout << "\n" << tokens.size() << " tokens produced." << std::endl;
  return 0;
}

int runParseCommand(const std::string &filepath,
                    const DebugCommandDependencies &deps) {
  const NeuronSettings settings = deps.loadNeuronSettings(std::filesystem::path(filepath));
  std::string source = deps.readFile(filepath, settings);
  if (source.empty()) {
    return 1;
  }

  std::vector<std::string> errors;
  std::vector<neuron::Token> tokens;
  bool configError = false;
  if (!expandDebugSource(std::filesystem::path(filepath), deps.toolRoot, source,
                         &configError, &errors, &tokens)) {
    deps.reportStringDiagnostics(configError ? "config" : "lexer", filepath, source,
                                 errors);
    return 1;
  }

  neuron::Parser parser(std::move(tokens), filepath);
  auto ast = parser.parse();
  if (!parser.errors().empty()) {
    deps.reportStringDiagnostics("parser", filepath, source, parser.errors());
    return 1;
  }

  neuron::SemanticAnalyzer sema;
  std::vector<std::string> configErrors;
  deps.configureSemanticAnalyzerModules(&sema, std::filesystem::path(filepath),
                                        deps.tryLoadProjectConfigFromCwd(),
                                        settings, source, &configErrors);
  if (!configErrors.empty()) {
    deps.reportStringDiagnostics("config", filepath, source, configErrors);
    return 1;
  }
  sema.analyze(ast.get());

  if (sema.hasErrors()) {
    deps.reportSemanticDiagnostics(filepath, source, sema.getErrors());
    return 1;
  }

  std::cout << "=== Semantic Analysis Success ===" << std::endl;
  std::cout << "  " << ast->declarations.size() << " top-level declarations."
            << std::endl;
  return 0;
}

int runNirCommand(const std::string &filepath,
                  const DebugCommandDependencies &deps) {
  const NeuronSettings settings = deps.loadNeuronSettings(std::filesystem::path(filepath));
  std::string source = deps.readFile(filepath, settings);
  if (source.empty()) {
    return 1;
  }

  std::vector<std::string> errors;
  std::vector<neuron::Token> tokens;
  bool configError = false;
  if (!expandDebugSource(std::filesystem::path(filepath), deps.toolRoot, source,
                         &configError, &errors, &tokens)) {
    deps.reportStringDiagnostics(configError ? "config" : "lexer", filepath, source,
                                 errors);
    return 1;
  }

  neuron::Parser parser(std::move(tokens), filepath);
  auto ast = parser.parse();
  if (!parser.errors().empty()) {
    deps.reportStringDiagnostics("parser", filepath, source, parser.errors());
    return 1;
  }

  neuron::SemanticAnalyzer sema;
  std::vector<std::string> configErrors;
  deps.configureSemanticAnalyzerModules(&sema, std::filesystem::path(filepath),
                                        deps.tryLoadProjectConfigFromCwd(),
                                        settings, source, &configErrors);
  if (!configErrors.empty()) {
    deps.reportStringDiagnostics("config", filepath, source, configErrors);
    return 1;
  }
  sema.analyze(ast.get());

  if (sema.hasErrors()) {
    deps.reportSemanticDiagnostics(filepath, source, sema.getErrors());
    return 1;
  }

  neuron::nir::NIRBuilder nirBuilder;
  auto module = nirBuilder.build(ast.get(), "module_" + filepath);

  std::cout << "\n=== Unoptimized NIR Output ===" << std::endl;
  module->print();

  auto optimizer = neuron::nir::Optimizer::createDefaultOptimizer();
  optimizer->run(module.get());

  std::cout << "\n=== Optimized NIR Output ===" << std::endl;
  module->print();

  return 0;
}

} // namespace neuron::cli

