#include "neuronc/lexer/Lexer.h"
#include <cctype>
#include <sstream>

namespace neuron {

// ── Keyword table ──
static const std::unordered_map<std::string, TokenType> g_keywords = {
    {"is",            TokenType::Is},
    {"another",       TokenType::Another},
    {"as",            TokenType::As},
    {"then",          TokenType::Then},
    {"maybe",         TokenType::Maybe},
    {"move",          TokenType::Move},
    {"method",        TokenType::Method},
    {"class",         TokenType::Class},
    {"struct",        TokenType::Struct},
    {"interface",     TokenType::Interface},
    {"enum",          TokenType::Enum},
    {"constructor",   TokenType::Constructor},
    {"inherits",      TokenType::Inherits},
    {"this",          TokenType::This},
    {"public",        TokenType::Public},
    {"private",       TokenType::Private},
    {"module",        TokenType::Module},
    {"expand",        TokenType::Expand},
    {"modulecpp",     TokenType::ModuleCpp},
    {"if",            TokenType::If},
    {"else",          TokenType::Else},
    {"match",         TokenType::Match},
    {"default",       TokenType::Default},
    {"while",         TokenType::While},
    {"for",           TokenType::For},
    {"in",            TokenType::In},
    {"parallel",      TokenType::Parallel},
    {"break",         TokenType::Break},
    {"continue",      TokenType::Continue},
    {"return",        TokenType::Return},
    {"try",           TokenType::Try},
    {"catch",         TokenType::Catch},
    {"finally",       TokenType::Finally},
    {"throw",         TokenType::Throw},
    {"async",         TokenType::Async},
    {"await",         TokenType::Await},
    {"thread",        TokenType::Thread},
    {"atomic",        TokenType::Atomic},
    {"gpu",           TokenType::Gpu},
    {"canvas",        TokenType::Canvas},
    {"shader",        TokenType::Shader},
    {"pass",          TokenType::Pass},
    {"unsafe",        TokenType::Unsafe},
    {"const",         TokenType::Const},
    {"constexpr",     TokenType::Constexpr},
    {"extern",        TokenType::Extern},
    {"abstract",      TokenType::Abstract},
    {"virtual",       TokenType::Virtual},
    {"override",      TokenType::Override},
    {"overload",      TokenType::Overload},
    {"macro",         TokenType::Macro},
    {"typeof",        TokenType::Typeof},
    {"static_assert", TokenType::StaticAssert},
    {"true",          TokenType::True},
    {"false",         TokenType::False},
    {"null",          TokenType::Null},
};

// ── Constructor ──
Lexer::Lexer(std::string source, std::string filename)
    : m_source(std::move(source)), m_filename(std::move(filename)) {}

// ── Character helpers ──
char Lexer::current() const {
    return isAtEnd() ? '\0' : m_source[m_pos];
}

char Lexer::peek() const {
    size_t next = m_pos + 1;
    return (next >= m_source.size()) ? '\0' : m_source[next];
}

char Lexer::advance() {
    char c = current();
    m_pos++;
    if (c == '\n') {
        m_line++;
        m_column = 1;
    } else {
        m_column++;
    }
    return c;
}

bool Lexer::isAtEnd() const {
    return m_pos >= m_source.size();
}

bool Lexer::match(char expected) {
    if (isAtEnd() || m_source[m_pos] != expected) return false;
    advance();
    return true;
}

// ── Whitespace & comments ──
void Lexer::skipWhitespace() {
    while (!isAtEnd()) {
        char c = current();
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
            advance();
        } else if (c == '/' && peek() == '/') {
            skipLineComment();
        } else if (c == '/' && peek() == '*') {
            skipBlockComment();
        } else {
            break;
        }
    }
}

void Lexer::skipLineComment() {
    advance(); // first /
    advance(); // second /
    while (!isAtEnd() && current() != '\n') {
        advance();
    }
}

void Lexer::skipBlockComment() {
    advance(); // /
    advance(); // *
    while (!isAtEnd()) {
        if (current() == '*' && peek() == '/') {
            advance(); // *
            advance(); // /
            return;
        }
        advance();
    }
    m_errors.push_back("Unterminated block comment");
}

// ── Token creation ──
Token Lexer::makeToken(TokenType type, const std::string& value) {
    return Token(type, value, {m_line, m_column, m_filename});
}

Token Lexer::makeToken(TokenType type, const std::string& value, SourceLocation loc) {
    return Token(type, value, std::move(loc));
}

Token Lexer::errorToken(const std::string& message) {
    std::ostringstream oss;
    oss << m_filename << ":" << m_line << ":" << m_column << ": " << message;
    m_errors.push_back(oss.str());
    return Token(TokenType::Error, message, {m_line, m_column, m_filename});
}

// ── Number scanning ──
Token Lexer::scanNumber() {
    SourceLocation loc = {m_line, m_column, m_filename};
    std::string num;
    bool isFloat = false;

    while (!isAtEnd() && std::isdigit(current())) {
        num += advance();
    }

    if (!isAtEnd() && current() == '.' && peek() != '.') {
        isFloat = true;
        num += advance(); // .
        while (!isAtEnd() && std::isdigit(current())) {
            num += advance();
        }
    }

    // Scientific notation
    if (!isAtEnd() && (current() == 'e' || current() == 'E')) {
        isFloat = true;
        num += advance();
        if (!isAtEnd() && (current() == '+' || current() == '-')) {
            num += advance();
        }
        while (!isAtEnd() && std::isdigit(current())) {
            num += advance();
        }
    }

    // Explicit float suffix
    if (!isAtEnd() && (current() == 'f' || current() == 'F')) {
        isFloat = true;
        advance(); // skip suffix, don't add to value
    }

    return makeToken(isFloat ? TokenType::FloatLiteral : TokenType::IntLiteral, num, loc);
}

// ── String scanning ──
Token Lexer::scanString() {
    SourceLocation loc = {m_line, m_column, m_filename};
    advance(); // opening "
    std::string str;

    while (!isAtEnd() && current() != '"') {
        if (current() == '\\') {
            advance();
            switch (current()) {
                case 'n':  str += '\n'; break;
                case 't':  str += '\t'; break;
                case 'r':  str += '\r'; break;
                case '\\': str += '\\'; break;
                case '"':  str += '"';  break;
                case '0':  str += '\0'; break;
                default:
                    str += '\\';
                    str += current();
                    break;
            }
            advance();
        } else {
            str += advance();
        }
    }

    if (isAtEnd()) {
        return errorToken("Unterminated string literal");
    }

    advance(); // closing "
    return makeToken(TokenType::StringLiteral, str, loc);
}

// ── Identifier / keyword scanning ──
Token Lexer::scanIdentifierOrKeyword() {
    SourceLocation loc = {m_line, m_column, m_filename};
    std::string word;

    while (!isAtEnd() && (std::isalnum(current()) || current() == '_')) {
        word += advance();
    }

    // Check for multi-word keywords: "address of", "value of", "static_assert"
    TokenType multiWord = checkMultiWordKeyword(word);
    if (multiWord != TokenType::Identifier) {
        return makeToken(multiWord, word, loc);
    }

    // Check single-word keywords
    auto it = g_keywords.find(word);
    if (it != g_keywords.end()) {
        return makeToken(it->second, word, loc);
    }

    return makeToken(TokenType::Identifier, word, loc);
}

// ── Multi-word keyword detection ──
// After scanning a word, peeks ahead to see if the next word
// forms a multi-word keyword like "address of" or "value of".
TokenType Lexer::checkMultiWordKeyword(const std::string& word) {
    if (word == "address" || word == "value") {
        // save state
        size_t savedPos = m_pos;
        int savedLine = m_line;
        int savedColumn = m_column;

        // skip whitespace
        while (m_pos < m_source.size() && (m_source[m_pos] == ' ' || m_source[m_pos] == '\t')) {
            m_pos++;
            m_column++;
        }

        // try to read "of"
        if (m_pos + 1 < m_source.size() && m_source[m_pos] == 'o' && m_source[m_pos + 1] == 'f') {
            // make sure "of" is not part of a longer word
            size_t afterOf = m_pos + 2;
            if (afterOf >= m_source.size() || (!std::isalnum(m_source[afterOf]) && m_source[afterOf] != '_')) {
                m_pos = afterOf;
                m_column += 2;
                if (word == "address") return TokenType::AddressOf;
                if (word == "value")   return TokenType::ValueOf;
            }
        }

        // restore state — not a multi-word keyword
        m_pos = savedPos;
        m_line = savedLine;
        m_column = savedColumn;
    }

    return TokenType::Identifier;
}

// ── Main scan routine ──
Token Lexer::scanToken() {
    skipWhitespace();

    if (isAtEnd()) {
        return makeToken(TokenType::Eof, "");
    }

    SourceLocation loc = {m_line, m_column, m_filename};
    char c = current();

    // Numbers
    if (std::isdigit(c)) {
        return scanNumber();
    }

    // Strings
    if (c == '"') {
        return scanString();
    }

    // Identifiers and keywords
    if (std::isalpha(c) || c == '_') {
        return scanIdentifierOrKeyword();
    }

    // Operators and punctuation
    advance();
    switch (c) {
        // Arithmetic
        case '+':
            if (match('+')) return makeToken(TokenType::PlusPlus,  "++", loc);
            return makeToken(TokenType::Plus, "+", loc);
        case '-':
            if (match('-')) return makeToken(TokenType::MinusMinus, "--", loc);
            return makeToken(TokenType::Minus, "-", loc);
        case '*':
            return makeToken(TokenType::Star, "*", loc);
        case '/':
            return makeToken(TokenType::Slash, "/", loc);
        case '^':
            if (match('^')) return makeToken(TokenType::CaretCaret, "^^", loc);
            return makeToken(TokenType::Caret, "^", loc);
        case '@':
            return makeToken(TokenType::At, "@", loc);

        // Comparison
        case '=':
            if (match('=')) return makeToken(TokenType::EqualEqual, "==", loc);
            return errorToken("Unexpected '='. Did you mean '==' or 'is'?");
        case '!':
            if (match('=')) return makeToken(TokenType::NotEqual, "!=", loc);
            return makeToken(TokenType::Not, "!", loc);
        case '>':
            if (match('=')) return makeToken(TokenType::GreaterEqual, ">=", loc);
            return makeToken(TokenType::Greater, ">", loc);
        case '<':
            if (match('=')) return makeToken(TokenType::LessEqual, "<=", loc);
            return makeToken(TokenType::Less, "<", loc);

        // Logical
        case '&':
            if (match('&')) return makeToken(TokenType::And, "&&", loc);
            return errorToken("Unexpected '&'. Did you mean '&&' or 'address of'?");
        case '|':
            if (match('|')) return makeToken(TokenType::Or, "||", loc);
            return errorToken("Unexpected '|'. Did you mean '||'?");

        // Punctuation
        case ';': return makeToken(TokenType::Semicolon,    ";", loc);
        case ':': return makeToken(TokenType::Colon,        ":", loc);
        case ',': return makeToken(TokenType::Comma,        ",", loc);
        case '.':
            if (match('.')) return makeToken(TokenType::DotDot, "..", loc);
            return makeToken(TokenType::Dot, ".", loc);
        case '(': return makeToken(TokenType::LeftParen,    "(", loc);
        case ')': return makeToken(TokenType::RightParen,   ")", loc);
        case '{': return makeToken(TokenType::LeftBrace,    "{", loc);
        case '}': return makeToken(TokenType::RightBrace,   "}", loc);
        case '[': return makeToken(TokenType::LeftBracket,  "[", loc);
        case ']': return makeToken(TokenType::RightBracket, "]", loc);

        default:
            return errorToken(std::string("Unexpected character: '") + c + "'");
    }
}

// ── Public tokenize ──
std::vector<Token> Lexer::tokenize() {
    m_tokens.clear();
    m_errors.clear();
    m_pos = 0;
    m_line = 1;
    m_column = 1;

    while (true) {
        Token token = scanToken();
        m_tokens.push_back(token);
        if (token.type == TokenType::Eof || token.type == TokenType::Error) {
            break;
        }
    }

    // Don't stop at first error — collect all tokens
    if (!m_tokens.empty() && m_tokens.back().type == TokenType::Error) {
        // Continue scanning to collect more tokens
        while (!isAtEnd()) {
            Token token = scanToken();
            m_tokens.push_back(token);
            if (token.type == TokenType::Eof) break;
        }
    }

    return m_tokens;
}

} // namespace neuron
