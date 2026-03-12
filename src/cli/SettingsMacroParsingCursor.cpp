#include "SettingsMacroInternal.h"

#include <cctype>

namespace neuron::cli {

char SettingsFileParser::current() const {
  return m_pos < m_text.size() ? m_text[m_pos] : '\0';
}

char SettingsFileParser::peek() const {
  const std::size_t next = m_pos + 1;
  return next < m_text.size() ? m_text[next] : '\0';
}

bool SettingsFileParser::isAtEnd() const { return m_pos >= m_text.size(); }

char SettingsFileParser::advance() {
  const char ch = current();
  ++m_pos;
  if (ch == '\n') {
    ++m_line;
    m_column = 1;
  } else {
    ++m_column;
  }
  return ch;
}

bool SettingsFileParser::match(char expected) {
  if (current() != expected) {
    return false;
  }
  advance();
  return true;
}

void SettingsFileParser::skipInlineSpaces() {
  while (!isAtEnd() &&
         (current() == ' ' || current() == '\t' || current() == '\r')) {
    advance();
  }
}

void SettingsFileParser::skipLineComment() {
  while (!isAtEnd() && current() != '\n') {
    advance();
  }
}

void SettingsFileParser::skipTrivia() {
  while (!isAtEnd()) {
    if (std::isspace(static_cast<unsigned char>(current())) != 0) {
      advance();
      continue;
    }
    if (current() == '#') {
      skipLineComment();
      continue;
    }
    if (current() == '/' && peek() == '/') {
      advance();
      advance();
      skipLineComment();
      continue;
    }
    return;
  }
}

bool SettingsFileParser::startsWithWord(const std::string &word) const {
  if (m_pos + word.size() > m_text.size() ||
      m_text.compare(m_pos, word.size(), word) != 0) {
    return false;
  }
  const std::size_t end = m_pos + word.size();
  if (end < m_text.size()) {
    const unsigned char next = static_cast<unsigned char>(m_text[end]);
    if (std::isalnum(next) != 0 || next == '_') {
      return false;
    }
  }
  return true;
}

void SettingsFileParser::consumeWord(const std::string &word) {
  for (char ignored : word) {
    (void)ignored;
    advance();
  }
}

std::string SettingsFileParser::parseIdentifier() {
  if (isAtEnd()) {
    return "";
  }
  const unsigned char first = static_cast<unsigned char>(current());
  if (std::isalpha(first) == 0 && current() != '_') {
    return "";
  }
  std::string out;
  while (!isAtEnd()) {
    const unsigned char ch = static_cast<unsigned char>(current());
    if (std::isalnum(ch) == 0 && current() != '_') {
      break;
    }
    out.push_back(advance());
  }
  return out;
}

bool SettingsFileParser::looksLikeMethodLiteral() const {
  std::size_t pos = m_pos;
  while (pos < m_text.size() &&
         std::isspace(static_cast<unsigned char>(m_text[pos])) != 0) {
    ++pos;
  }
  auto startsWordAt = [&](std::size_t at, const std::string &word) {
    if (at + word.size() > m_text.size() ||
        m_text.compare(at, word.size(), word) != 0) {
      return false;
    }
    const std::size_t end = at + word.size();
    if (end < m_text.size()) {
      const unsigned char next = static_cast<unsigned char>(m_text[end]);
      if (std::isalnum(next) != 0 || next == '_') {
        return false;
      }
    }
    return true;
  };

  if (startsWordAt(pos, "method")) {
    return true;
  }
  if (!startsWordAt(pos, "async")) {
    return false;
  }
  pos += std::string("async").size();
  while (pos < m_text.size() &&
         std::isspace(static_cast<unsigned char>(m_text[pos])) != 0) {
    ++pos;
  }
  return startsWordAt(pos, "method");
}

void SettingsFileParser::addError(std::vector<std::string> *outErrors, int line,
                                  int column, const std::string &message) {
  if (outErrors != nullptr) {
    outErrors->push_back(makeConfigError(m_path, line, column, message));
  }
}

} // namespace neuron::cli
