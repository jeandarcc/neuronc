#pragma once
#include <string>
#include <string_view>
#include <ostream>
#include <unordered_map>

namespace neuron {

// ── Token type enumeration ──
enum class TokenType {
    // Literals
    IntLiteral,
    FloatLiteral,
    StringLiteral,
    Null,

    // Identifier
    Identifier,

    // Binding keywords
    Is,           // is
    Another,      // another
    AddressOf,    // address of
    ValueOf,      // value of
    As,           // as
    Then,         // then
    Maybe,        // maybe
    Move,         // move

    // Method / class
    Method,       // method
    Class,        // class
    Struct,       // struct
    Interface,    // interface
    Enum,         // enum
    Constructor,  // constructor
    Inherits,     // inherits
    This,         // this

    // Access modifiers
    Public,       // public
    Private,      // private

    // Module
    Module,       // module
    Expand,       // expand
    ModuleCpp,    // modulecpp

    // Control flow
    If,           // if
    Else,         // else
    Match,        // match
    Default,      // default
    While,        // while
    For,          // for
    In,           // in
    Parallel,     // parallel
    Break,        // break
    Continue,     // continue
    Return,       // return

    // Error handling
    Try,          // try
    Catch,        // catch
    Finally,      // finally
    Throw,        // throw

    // Async
    Async,        // async
    Await,        // await

    // Concurrency
    Thread,       // thread
    Atomic,       // atomic
    Gpu,          // gpu
    Canvas,       // canvas
    Shader,       // shader
    Pass,         // pass

    // Safety
    Unsafe,       // unsafe
    Const,        // const
    Constexpr,    // constexpr
    Extern,       // extern
    Abstract,     // abstract
    Virtual,      // virtual
    Override,     // override
    Overload,     // overload

    // Metaprogramming
    Macro,        // macro
    Typeof,       // typeof
    StaticAssert, // static_assert

    // Boolean literals
    True,         // true
    False,        // false

    // Operators - Arithmetic
    Plus,         // +
    Minus,        // -
    Star,         // *
    Slash,        // /
    Caret,        // ^
    CaretCaret,   // ^^
    At,           // @ (matrix multiply)
    PlusPlus,     // ++
    MinusMinus,   // --

    // Operators - Comparison
    EqualEqual,   // ==
    NotEqual,     // !=
    Greater,      // >
    Less,         // <
    GreaterEqual, // >=
    LessEqual,    // <=

    // Operators - Logical
    And,          // &&
    Or,           // ||
    Not,          // !

    // Punctuation
    Semicolon,    // ;
    Colon,        // :
    Comma,        // ,
    Dot,          // .
    DotDot,       // ..
    LeftParen,    // (
    RightParen,   // )
    LeftBrace,    // {
    RightBrace,   // }
    LeftBracket,  // [
    RightBracket, // ]

    // Special
    Eof,
    Error,
};

// ── Source location ──
struct SourceLocation {
    int line   = 1;
    int column = 1;
    std::string file;
};

// ── Token ──
struct Token {
    TokenType     type     = TokenType::Eof;
    std::string   value;
    SourceLocation location;

    Token() = default;
    Token(TokenType t, std::string v, SourceLocation loc)
        : type(t), value(std::move(v)), location(std::move(loc)) {}
};

// ── Helpers ──
const char* tokenTypeName(TokenType type);
std::ostream& operator<<(std::ostream& os, const Token& token);

} // namespace neuron
