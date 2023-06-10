#pragma once
#include "Types.h"

#include <string>

enum class TokenType
{
    // Single-character tokens.
    LeftParen = 0x00,
    RightParen = 0x01,
    LeftBrace = 0x02,
    RightBrace = 0x03,
    Comma = 0x04,
    Dot = 0x05,
    Minus = 0x06,
    Plus = 0x07,
    Semicolon = 0x08,
    Slash = 0x09,
    Star = 0x0a,

    // One or two character tokens.
    Bang = 0x0b,
    BangEqual = 0x0c,
    Equal = 0x0d,
    EqualEqual = 0x0e,
    Greater = 0x0f,
    GreaterEqual = 0x10,
    Less = 0x11,
    LessEqual = 0x12,

    // Literals.
    Identifier = 0x13,
    String = 0x14,
    Number = 0x15,

    // Keywords.
    And = 0x16,
    Class = 0x17,
    Else = 0x18,
    False = 0x19,
    Fun = 0x1a,
    For = 0x1b,
    If = 0x1c,
    Nil = 0x1d,
    Or = 0x1e,
    Print = 0x1f,
    Return = 0x20,
    Super = 0x21,
    This = 0x22,
    True = 0x23,
    Var = 0x24,
    While = 0x25,

    Eof = 0x26,
    Error = 0x27,
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
