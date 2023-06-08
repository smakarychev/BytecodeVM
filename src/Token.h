#pragma once
#include "Types.h"

#include <string>

enum class TokenType
{
    // Single-character tokens.
    LeftParen, RightParen, LeftBrace, RightBrace,
    Comma, Dot, Minus, Plus, Semicolon, Slash, Star,

    // One or two character tokens.
    Bang, BangEqual,
    Equal, EqualEqual,
    Greater, GreaterEqual,
    Less, LessEqual,

    // Literals.
    Identifier, String, Number,

    // Keywords.
    And, Class, Else, False, Fun, For, If, Nil, Or,
    Print, Return, Super, This, True, Var, While,

    Eof, Error
};

struct TokenUtils
{
    static std::string TokenTypeToString(TokenType type); 
};

struct Token
{
    TokenType Type;
    std::string_view Lexeme;
    u32 Line;

    std::string ToString() const;
    friend std::ostream& operator <<(std::ostream& out, const Token& token);
};
