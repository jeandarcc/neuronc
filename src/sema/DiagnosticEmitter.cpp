#include "DiagnosticEmitter.h"

#include <algorithm>
#include <cctype>
#include <sstream>

namespace neuron::sema_detail {

void DiagnosticEmitter::reset() { m_errors.clear(); }

void DiagnosticEmitter::setSourceText(std::string sourceText) {
  m_sourceLines.clear();
  std::istringstream in(sourceText);
  std::string line;
  while (std::getline(in, line)) {
    m_sourceLines.push_back(std::move(line));
  }
  if (!sourceText.empty() && sourceText.back() == '\n') {
    m_sourceLines.emplace_back();
  }
}

void DiagnosticEmitter::setAgentHints(bool enabled) { m_agentHints = enabled; }

void DiagnosticEmitter::emit(const SourceLocation &loc,
                             const std::string &message) {
  SemanticError error;
  error.message = message;
  error.location = loc;
  m_errors.push_back(std::move(error));
}

void DiagnosticEmitter::emit(const SourceLocation &loc, const std::string &code,
                             diagnostics::DiagnosticArguments arguments,
                             const std::string &message) {
  SemanticError error;
  error.message = message;
  error.code = code;
  error.arguments = std::move(arguments);
  error.location = loc;
  m_errors.push_back(std::move(error));
}

void DiagnosticEmitter::emitWithAgentHint(const SourceLocation &loc,
                                          const std::string &message,
                                          std::string_view hint) {
  if (!m_agentHints || hint.empty()) {
    emit(loc, message);
    return;
  }
  emit(loc, message + " For agents: " + std::string(hint));
}

const std::vector<SemanticError> &DiagnosticEmitter::errors() const {
  return m_errors;
}

bool DiagnosticEmitter::hasErrors() const { return !m_errors.empty(); }

bool DiagnosticEmitter::hasRequiredPublicMethodDocs(
    const MethodDeclNode *node) const {
  if (node == nullptr || node->location.line <= 1 || m_sourceLines.empty()) {
    return false;
  }

  int lineIndex = node->location.line - 2;
  if (lineIndex >= static_cast<int>(m_sourceLines.size())) {
    lineIndex = static_cast<int>(m_sourceLines.size()) - 1;
  }
  while (lineIndex >= 0 &&
         trimCopy(m_sourceLines[static_cast<size_t>(lineIndex)]).empty()) {
    --lineIndex;
  }

  bool sawSummaryStart = false;
  bool sawSummaryEnd = false;

  for (; lineIndex >= 0; --lineIndex) {
    const std::string line =
        trimCopy(m_sourceLines[static_cast<size_t>(lineIndex)]);
    if (line.empty()) {
      continue;
    }
    if (line.rfind("///", 0) != 0) {
      break;
    }
    if (line.find("<summary>") != std::string::npos) {
      sawSummaryStart = true;
    }
    if (line.find("</summary>") != std::string::npos) {
      sawSummaryEnd = true;
    }
  }

  return sawSummaryStart && sawSummaryEnd;
}

std::string DiagnosticEmitter::trimCopy(std::string text) {
  auto notSpace = [](unsigned char c) { return !std::isspace(c); };
  text.erase(text.begin(),
             std::find_if(text.begin(), text.end(), notSpace));
  text.erase(std::find_if(text.rbegin(), text.rend(), notSpace).base(),
             text.end());
  return text;
}

} // namespace neuron::sema_detail
