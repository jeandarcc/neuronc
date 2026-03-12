#include "neuronc/lexer/Token.h"

namespace neuron {

const char* tokenTypeName(TokenType type) {
    switch (type) {
        case TokenType::IntLiteral:    return "IntLiteral";
        case TokenType::FloatLiteral:  return "FloatLiteral";
        case TokenType::StringLiteral: return "StringLiteral";
        case TokenType::Identifier:    return "Identifier";
        case TokenType::Is:            return "Is";
        case TokenType::Another:       return "Another";
        case TokenType::AddressOf:     return "AddressOf";
        case TokenType::ValueOf:       return "ValueOf";
        case TokenType::As:            return "As";
        case TokenType::Then:          return "Then";
        case TokenType::Maybe:         return "Maybe";
        case TokenType::Move:          return "Move";
        case TokenType::Method:        return "Method";
        case TokenType::Class:         return "Class";
        case TokenType::Struct:        return "Struct";
        case TokenType::Interface:     return "Interface";
        case TokenType::Enum:          return "Enum";
        case TokenType::Constructor:   return "Constructor";
        case TokenType::Inherits:      return "Inherits";
        case TokenType::This:          return "This";
        case TokenType::Public:        return "Public";
        case TokenType::Private:       return "Private";
        case TokenType::Module:        return "Module";
        case TokenType::ModuleCpp:     return "ModuleCpp";
        case TokenType::If:            return "If";
        case TokenType::Else:          return "Else";
        case TokenType::Match:         return "Match";
        case TokenType::Default:       return "Default";
        case TokenType::While:         return "While";
        case TokenType::For:           return "For";
        case TokenType::In:            return "In";
        case TokenType::Parallel:      return "Parallel";
        case TokenType::Break:         return "Break";
        case TokenType::Continue:      return "Continue";
        case TokenType::Return:        return "Return";
        case TokenType::Try:           return "Try";
        case TokenType::Catch:         return "Catch";
        case TokenType::Finally:       return "Finally";
        case TokenType::Throw:         return "Throw";
        case TokenType::Async:         return "Async";
        case TokenType::Await:         return "Await";
        case TokenType::Thread:        return "Thread";
        case TokenType::Atomic:        return "Atomic";
        case TokenType::Gpu:           return "Gpu";
        case TokenType::Canvas:        return "Canvas";
        case TokenType::Shader:        return "Shader";
        case TokenType::Pass:          return "Pass";
        case TokenType::Unsafe:        return "Unsafe";
        case TokenType::Const:         return "Const";
        case TokenType::Constexpr:     return "Constexpr";
        case TokenType::Extern:        return "Extern";
        case TokenType::Abstract:      return "Abstract";
        case TokenType::Virtual:       return "Virtual";
        case TokenType::Override:      return "Override";
        case TokenType::Overload:      return "Overload";
        case TokenType::Macro:         return "Macro";
        case TokenType::Typeof:        return "Typeof";
        case TokenType::StaticAssert:  return "StaticAssert";
        case TokenType::True:          return "True";
        case TokenType::False:         return "False";
        case TokenType::Plus:          return "Plus";
        case TokenType::Minus:         return "Minus";
        case TokenType::Star:          return "Star";
        case TokenType::Slash:         return "Slash";
        case TokenType::At:            return "At";
        case TokenType::PlusPlus:      return "PlusPlus";
        case TokenType::MinusMinus:    return "MinusMinus";
        case TokenType::EqualEqual:    return "EqualEqual";
        case TokenType::NotEqual:      return "NotEqual";
        case TokenType::Greater:       return "Greater";
        case TokenType::Less:          return "Less";
        case TokenType::GreaterEqual:  return "GreaterEqual";
        case TokenType::LessEqual:     return "LessEqual";
        case TokenType::And:           return "And";
        case TokenType::Or:            return "Or";
        case TokenType::Not:           return "Not";
        case TokenType::Semicolon:     return "Semicolon";
        case TokenType::Colon:         return "Colon";
        case TokenType::Comma:         return "Comma";
        case TokenType::Dot:           return "Dot";
        case TokenType::DotDot:        return "DotDot";
        case TokenType::LeftParen:     return "LeftParen";
        case TokenType::RightParen:    return "RightParen";
        case TokenType::LeftBrace:     return "LeftBrace";
        case TokenType::RightBrace:    return "RightBrace";
        case TokenType::LeftBracket:   return "LeftBracket";
        case TokenType::RightBracket:  return "RightBracket";
        case TokenType::Eof:           return "Eof";
        case TokenType::Error:         return "Error";
    }
    return "Unknown";
}

std::ostream& operator<<(std::ostream& os, const Token& token) {
    os << tokenTypeName(token.type);
    if (!token.value.empty()) {
        os << "(" << token.value << ")";
    }
    os << " [" << token.location.line << ":" << token.location.column << "]";
    return os;
}

} // namespace neuron
