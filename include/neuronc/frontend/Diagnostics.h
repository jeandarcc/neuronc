#pragma once

#include "neuronc/diagnostics/DiagnosticFormat.h"
#include "neuronc/parser/Parser.h"
#include "neuronc/sema/SemanticAnalyzer.h"

#include <string>
#include <vector>

namespace neuron::frontend {

enum class DiagnosticSeverity {
  Error,
  Warning,
};

struct Position {
  int line = 1;
  int column = 1;
};

struct Range {
  Position start;
  Position end;
};

struct Diagnostic {
  std::string phase;
  DiagnosticSeverity severity = DiagnosticSeverity::Error;
  std::string code;
  std::string file;
  Range range;
  std::string message;
  neuron::diagnostics::DiagnosticArguments arguments;
};

std::vector<Diagnostic>
convertStringDiagnostics(const std::string &phase, const std::string &filepath,
                         const std::vector<std::string> &diagnostics);

std::vector<Diagnostic>
convertParserDiagnostics(const std::string &filepath,
                         const std::vector<neuron::ParserDiagnostic> &diagnostics);

std::vector<Diagnostic>
convertSemanticDiagnostics(const std::vector<neuron::SemanticError> &diagnostics);

} // namespace neuron::frontend
