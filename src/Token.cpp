#include "Token.h"

#include "Log.h"

// not the best way to do it, but I've decided to keep it simple
std::string TokenUtils::TokenTypeToString(TokenType type)
{
    switch (type)
    {
    case TokenType::LeftParen:      return "LeftParen";
    case TokenType::RightParen:     return "RightParen";
    case TokenType::LeftBrace:      return "LeftBrace";
    case TokenType::RightBrace:     return "RightBrace";
    case TokenType::LeftSquare:     return "LeftSquare";
    case TokenType::RightSquare:    return "RightSquare";
    case TokenType::Comma:          return "Comma";
    case TokenType::Dot:            return "Dot";
    case TokenType::Minus:          return "Minus";
    case TokenType::Plus:           return "Plus";
    case TokenType::Semicolon:      return "Semicolon";
    case TokenType::Slash:          return "Slash";
    case TokenType::Star:           return "Star";
    case TokenType::Pipe:           return "Pipe";
    case TokenType::Bang:           return "Bang";
    case TokenType::BangEqual:      return "BangEqual";
    case TokenType::Equal:          return "Equal";
    case TokenType::EqualEqual:     return "EqualEqual";
    case TokenType::Greater:        return "Greater";
    case TokenType::GreaterEqual:   return "GreaterEqual";
    case TokenType::Less:           return "Less";
    case TokenType::LessEqual:      return "LessEqual";
    case TokenType::Identifier:     return "Identifier";
    case TokenType::String:         return "String";
    case TokenType::Number:         return "Number";
    case TokenType::And:            return "And";
    case TokenType::Class:          return "Class";
    case TokenType::Else:           return "Else";
    case TokenType::False:          return "False";
    case TokenType::Fun:            return "Fun";
    case TokenType::For:            return "For";
    case TokenType::If:             return "If";
    case TokenType::Nil:            return "Nil";
    case TokenType::Or:             return "Or";
    case TokenType::Print:          return "Print";
    case TokenType::Return:         return "Return";
    case TokenType::Super:          return "Super";
    case TokenType::This:           return "This";
    case TokenType::True:           return "True";
    case TokenType::Var:            return "Var";
    case TokenType::While:          return "While";
    case TokenType::Eof:            return "Eof";
    case TokenType::Error:          return "Error";
    }
    LOG_FATAL("Unknown TokenType");
    return {};
}

std::string Token::ToString() const
{
    return std::to_string(Line) + " " + TokenUtils::TokenTypeToString(Type) + " " + std::string{Lexeme};
}

std::ostream& operator<<(std::ostream& out, const Token& token)
{
    out << token.ToString();
    return out;
}
