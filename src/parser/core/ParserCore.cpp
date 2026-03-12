#include "neuronc/parser/Parser.h"

#include <sstream>

namespace neuron {

namespace {

std::string syntheticTokenValue(TokenType type) {
  switch (type) {
  case TokenType::Identifier:
    return "__missing__";
  case TokenType::Semicolon:
    return ";";
  case TokenType::Colon:
    return ":";
  case TokenType::Comma:
    return ",";
  case TokenType::Dot:
    return ".";
  case TokenType::DotDot:
    return "..";
  case TokenType::LeftParen:
    return "(";
  case TokenType::RightParen:
    return ")";
  case TokenType::LeftBrace:
    return "{";
  case TokenType::RightBrace:
    return "}";
  case TokenType::LeftBracket:
    return "[";
  case TokenType::RightBracket:
    return "]";
  case TokenType::Greater:
    return ">";
  case TokenType::Less:
    return "<";
  case TokenType::Method:
    return "method";
  case TokenType::Class:
    return "class";
  case TokenType::Struct:
    return "struct";
  case TokenType::Interface:
    return "interface";
  case TokenType::Enum:
    return "enum";
  case TokenType::Shader:
    return "shader";
  case TokenType::Module:
    return "module";
  case TokenType::ModuleCpp:
    return "modulecpp";
  case TokenType::As:
    return "as";
  case TokenType::Is:
    return "is";
  case TokenType::Then:
    return "then";
  case TokenType::Catch:
    return "catch";
  case TokenType::Finally:
    return "finally";
  case TokenType::Default:
    return "default";
  default:
    return "";
  }
}

} // namespace

const Token &Parser::current() const {
  static Token eofToken(TokenType::Eof, "", {0, 0, ""});
  return (m_pos < m_tokens.size()) ? m_tokens[m_pos] : eofToken;
}

const Token &Parser::peek() const {
  static Token eofToken(TokenType::Eof, "", {0, 0, ""});
  size_t next = m_pos + 1;
  return (next < m_tokens.size()) ? m_tokens[next] : eofToken;
}

const Token &Parser::lookahead(std::size_t offset) const {
  static Token eofToken(TokenType::Eof, "", {0, 0, ""});
  const size_t index = m_pos + offset;
  return (index < m_tokens.size()) ? m_tokens[index] : eofToken;
}

const Token &Parser::advance() {
  const Token &tok = current();
  if (m_pos < m_tokens.size())
    m_pos++;
  return tok;
}

bool Parser::check(TokenType type) const { return current().type == type; }

bool Parser::match(TokenType type) {
  if (check(type)) {
    advance();
    return true;
  }
  return false;
}

Token Parser::expect(TokenType type, const std::string &message) {
  if (check(type)) {
    return advance();
  }
  error(message + " (got '" + current().value + "')");
  return Token(type, syntheticTokenValue(type), current().location);
}

bool Parser::isAtEnd() const { return current().type == TokenType::Eof; }

void Parser::error(const std::string &message) {
  auto &loc = current().location;
  std::ostringstream oss;
  oss << m_filename << ":" << loc.line << ":" << loc.column
      << ": error: " << message;
  m_errors.push_back(oss.str());
  m_diagnostics.push_back({"", {}, loc, message});
}

void Parser::error(const std::string &code,
                   diagnostics::DiagnosticArguments arguments,
                   const std::string &detail) {
  auto &loc = current().location;
  std::ostringstream oss;
  oss << m_filename << ":" << loc.line << ":" << loc.column
      << ": error: " << detail;
  m_errors.push_back(oss.str());
  m_diagnostics.push_back({code, std::move(arguments), loc, detail});
}

void Parser::synchronize() {
  advance();
  while (!isAtEnd()) {
    if (current().type == TokenType::Semicolon) {
      advance();
      return;
    }
    if (current().type == TokenType::RightBrace) {
      return;
    }
    switch (current().type) {
    case TokenType::Module:
    case TokenType::ModuleCpp:
    case TokenType::Class:
    case TokenType::Method:
    case TokenType::If:
    case TokenType::Match:
    case TokenType::Default:
    case TokenType::While:
    case TokenType::For:
    case TokenType::Return:
    case TokenType::Try:
    case TokenType::Gpu:
    case TokenType::Canvas:
    case TokenType::Shader:
      return;
    default:
      advance();
    }
  }
}

void Parser::recoverNoProgress() {
  if (isAtEnd() || check(TokenType::RightBrace)) {
    return;
  }
  if (check(TokenType::Semicolon)) {
    advance();
    return;
  }
  synchronize();
}

} // namespace neuron

