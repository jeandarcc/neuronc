#pragma once
#include "neuronc/lexer/Token.h"
#include <string>
#include <vector>

namespace neuron {

class Lexer {
public:
    explicit Lexer(std::string source, std::string filename = "<input>");

    /// Tokenize the entire source and return the token list.
    std::vector<Token> tokenize();

    /// Get accumulated error messages.
    const std::vector<std::string>& errors() const { return m_errors; }

private:
    // Character-level helpers
    char current() const;
    char peek() const;
    char advance();
    bool isAtEnd() const;
    bool match(char expected);

    // Token-level helpers
    void skipWhitespace();
    void skipLineComment();
    void skipBlockComment();
    Token makeToken(TokenType type, const std::string& value);
    Token makeToken(TokenType type, const std::string& value, SourceLocation loc);
    Token errorToken(const std::string& message);

    // Scanning routines
    Token scanToken();
    Token scanNumber();
    Token scanString();
    Token scanIdentifierOrKeyword();

    // Multi-word keyword support
    TokenType checkMultiWordKeyword(const std::string& word);

    // Source data
    std::string m_source;
    std::string m_filename;
    size_t      m_pos    = 0;
    int         m_line   = 1;
    int         m_column = 1;

    // Pending multi-word state
    std::vector<Token> m_tokens;
    std::vector<std::string> m_errors;
};

} // namespace neuron
