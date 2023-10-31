#pragma once
#include "Types.h"

#include <string>

enum class TokenType
{
    // Single-character tokens.
    LeftParen,
    RightParen,
    LeftBrace,
    RightBrace,
    LeftSquare,
    RightSquare,
    Comma,
    Dot,
    Minus,
    Plus,
    Semicolon,
    Slash,
    Star,
    Pipe,

    // One or two character tokens.
    Bang,
    BangEqual,
    Equal,
    EqualEqual,
    Greater,
    GreaterEqual,
    Less,
    LessEqual,

    // Literals.
    Identifier,
    String,
    Number,

    // Keywords.
    And,
    Class,
    Else,
    False,
    Fun,
    For,
    If,
    Nil,
    Or,
    Return,
    Super,
    This,
    True,
    Let,
    While,

    Eof,
    Error,
};

struct TokenUtils
{
    static std::string TokenTypeToString(TokenType type); 
};

struct Token
{
    Token();
    Token(TokenType type, std::string_view lexeme, u32 line);
    TokenType Type{TokenType::Error};
    std::string_view Lexeme{""};
    u32 Line{0};

    std::string ToString() const;
    friend std::ostream& operator <<(std::ostream& out, const Token& token);
};
