#include "neuronc/parser/Parser.h"

namespace neuron {

Parser::Parser(std::vector<Token> tokens, std::string filename)
    : m_tokens(std::move(tokens)), m_filename(std::move(filename)) {}

ASTNodePtr Parser::takePendingDeclaration() {
  if (m_pendingDeclarations.empty()) {
    return nullptr;
  }
  ASTNodePtr node = std::move(m_pendingDeclarations.front());
  m_pendingDeclarations.erase(m_pendingDeclarations.begin());
  return node;
}

void Parser::queuePendingDeclaration(ASTNodePtr node) {
  if (node != nullptr) {
    m_pendingDeclarations.push_back(std::move(node));
  }
}

} // namespace neuron
