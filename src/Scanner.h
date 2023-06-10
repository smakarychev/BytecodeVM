#pragma once
#include <vector>

#include "Token.h"
#include "Types.h"

class Scanner
{
public:
    Scanner(std::string_view source);
    const std::vector<Token>& ScanTokens();
    bool HadError() const { return m_HadError; }
private:
    void ScanToken();
    bool IsAtEnd() const;
    char Advance();
    char Peek() const;
    char PeekNext() const;
    bool Match(char expected);
    bool IsAlpha(char c) const;
        
    void String();
    void Number();
    void Identifier();
    void CheckKeyword(std::string_view identifier, std::string_view keyword, TokenType match);
    
    void ConsumeComment();
    void ConsumeBlockComment();
    
    void AddToken(TokenType type);
    void AddErrorToken(std::string_view message);
private:
    std::string_view m_Source;
    std::vector<Token> m_Tokens;
    u32 m_Start{0};
    u32 m_Current{0};
    u32 m_Line{1};

    bool m_HadError{false};
};
