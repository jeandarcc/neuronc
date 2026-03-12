#include "SettingsMacroInternal.h"

namespace neuron::cli {

std::string SettingsFileParser::parseExpressionValue(
    std::vector<std::string> *outErrors) {
  std::string out;
  int parenDepth = 0;
  int bracketDepth = 0;
  int braceDepth = 0;
  bool inString = false;
  bool escape = false;
  bool inLineComment = false;
  bool inBlockComment = false;
  const int startLine = m_line;
  const int startColumn = m_column;

  while (!isAtEnd()) {
    const char ch = advance();
    out.push_back(ch);

    if (inLineComment) {
      if (ch == '\n') {
        inLineComment = false;
      } else {
        out.pop_back();
      }
      continue;
    }
    if (inBlockComment) {
      out.pop_back();
      if (ch == '*' && current() == '/') {
        advance();
        inBlockComment = false;
      }
      continue;
    }
    if (inString) {
      if (escape) {
        escape = false;
      } else if (ch == '\\') {
        escape = true;
      } else if (ch == '"') {
        inString = false;
      }
      continue;
    }

    if (ch == '"') {
      inString = true;
      continue;
    }
    if (ch == '#' && parenDepth == 0 && bracketDepth == 0 && braceDepth == 0) {
      out.pop_back();
      inLineComment = true;
      continue;
    }
    if (ch == '/' && current() == '/') {
      out.pop_back();
      advance();
      inLineComment = true;
      continue;
    }
    if (ch == '/' && current() == '*') {
      out.pop_back();
      advance();
      inBlockComment = true;
      continue;
    }

    switch (ch) {
    case '(':
      ++parenDepth;
      break;
    case ')':
      --parenDepth;
      break;
    case '[':
      ++bracketDepth;
      break;
    case ']':
      --bracketDepth;
      break;
    case '{':
      ++braceDepth;
      break;
    case '}':
      --braceDepth;
      break;
    case ';':
      if (parenDepth == 0 && bracketDepth == 0 && braceDepth == 0) {
        out.pop_back();
        return trimCopy(out);
      }
      break;
    default:
      break;
    }
  }

  addError(outErrors, startLine, startColumn,
           "Expression setting must end with ';'.");
  return "";
}

std::string SettingsFileParser::parseMethodValue(
    std::vector<std::string> *outErrors) {
  std::string out;
  bool inString = false;
  bool escape = false;
  bool inLineComment = false;
  bool inBlockComment = false;
  bool seenBody = false;
  int braceDepth = 0;
  const int startLine = m_line;
  const int startColumn = m_column;

  while (!isAtEnd()) {
    const char ch = advance();
    out.push_back(ch);

    if (inLineComment) {
      if (ch == '\n') {
        inLineComment = false;
      } else {
        out.pop_back();
      }
      continue;
    }
    if (inBlockComment) {
      out.pop_back();
      if (ch == '*' && current() == '/') {
        advance();
        inBlockComment = false;
      }
      continue;
    }
    if (inString) {
      if (escape) {
        escape = false;
      } else if (ch == '\\') {
        escape = true;
      } else if (ch == '"') {
        inString = false;
      }
      continue;
    }

    if (ch == '"') {
      inString = true;
      continue;
    }
    if (ch == '#') {
      out.pop_back();
      inLineComment = true;
      continue;
    }
    if (ch == '/' && current() == '/') {
      out.pop_back();
      advance();
      inLineComment = true;
      continue;
    }
    if (ch == '/' && current() == '*') {
      out.pop_back();
      advance();
      inBlockComment = true;
      continue;
    }
    if (ch == '{') {
      seenBody = true;
      ++braceDepth;
      continue;
    }
    if (ch != '}') {
      continue;
    }

    --braceDepth;
    if (!seenBody || braceDepth != 0) {
      continue;
    }

    const std::size_t savedPos = m_pos;
    const int savedLine = m_line;
    const int savedColumn = m_column;
    skipInlineSpaces();
    if (current() == ';') {
      advance();
    } else {
      m_pos = savedPos;
      m_line = savedLine;
      m_column = savedColumn;
    }
    return trimCopy(out);
  }

  addError(outErrors, startLine, startColumn,
           "Method setting must contain a complete method body.");
  return "";
}

} // namespace neuron::cli
