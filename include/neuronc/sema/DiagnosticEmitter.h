#pragma once

#include "neuronc/diagnostics/DiagnosticFormat.h"
#include "neuronc/sema/SemanticAnalyzer.h"

namespace neuron::sema_detail {

class DiagnosticEmitter {
public:
  void reset();
  void setSourceText(std::string sourceText);
  void setAgentHints(bool enabled);

  void emit(const SourceLocation &loc, const std::string &message);
  void emit(const SourceLocation &loc, const std::string &code,
            diagnostics::DiagnosticArguments arguments,
            const std::string &message);
  void emitWithAgentHint(const SourceLocation &loc, const std::string &message,
                         std::string_view hint);

  const std::vector<SemanticError> &errors() const;
  bool hasErrors() const;
  bool hasRequiredPublicMethodDocs(const MethodDeclNode *node) const;

private:
  static std::string trimCopy(std::string text);

  std::vector<SemanticError> m_errors;
  std::vector<std::string> m_sourceLines;
  bool m_agentHints = false;
};

} // namespace neuron::sema_detail
