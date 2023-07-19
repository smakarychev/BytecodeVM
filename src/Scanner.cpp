#include "Scanner.h"

Scanner::Scanner(std::string_view source)
    : m_Source(source)
{
}

const std::vector<Token>& Scanner::ScanTokens()
{
    while (!IsAtEnd())
    {
        // start new lexeme
        m_Start = m_Current;
        ScanToken();
    }
    m_Start = m_Current;
    AddToken(TokenType::Eof);
    return m_Tokens;
}

void Scanner::ScanToken()
{
    char c = Advance();
    switch (c)
    {
    case '(': AddToken(TokenType::LeftParen); break;
    case ')': AddToken(TokenType::RightParen); break;
    case '{': AddToken(TokenType::LeftBrace); break;
    case '}': AddToken(TokenType::RightBrace); break;
    case '[': AddToken(TokenType::LeftSquare); break;
    case ']': AddToken(TokenType::RightSquare); break;
    case ',': AddToken(TokenType::Comma); break;
    case '.': AddToken(TokenType::Dot); break;
    case '-': AddToken(TokenType::Minus); break;
    case '+': AddToken(TokenType::Plus); break;
    case ';': AddToken(TokenType::Semicolon); break;
    case '*': AddToken(TokenType::Star); break;
    case '|': AddToken(TokenType::Pipe); break;
    case '!': AddToken(Match('=') ? TokenType::BangEqual : TokenType::Bang); break;
    case '=': AddToken(Match('=') ? TokenType::EqualEqual : TokenType::Equal); break;
    case '<': AddToken(Match('=') ? TokenType::LessEqual : TokenType::Less); break;
    case '>': AddToken(Match('=') ? TokenType::GreaterEqual : TokenType::Greater); break;
    case '/':
        if (Peek() == '/') ConsumeComment();
        else if (Peek() == '*') ConsumeBlockComment();
        else AddToken(TokenType::Slash);
        break;
    // skip spaces and tabs
    case ' ':
    case '\r':
    case '\t':
        break;
    case '\n':
        m_Line++;
        break;
    case '"':
    case '\'':
        String(c); break;
    default:
        if (std::isdigit(c)) Number();
        else if (IsAlpha(c)) Identifier();
        else  AddErrorToken("Unexpected character.");
        break;
    }
}

void Scanner::AddToken(TokenType type)
{
    m_Tokens.emplace_back(type, m_Source.substr(m_Start, m_Current - m_Start), m_Line);
}

void Scanner::AddErrorToken(std::string_view message)
{
    m_HadError = true;
    m_Tokens.emplace_back(TokenType::Error, message, m_Line);
}

bool Scanner::IsAtEnd() const
{
    return m_Current >= m_Source.length();
}

char Scanner::Advance()
{
    return m_Source[m_Current++];
}

char Scanner::Peek() const
{
    if (IsAtEnd()) return '\0';
    return m_Source[m_Current];
}

char Scanner::PeekNext() const
{
    if (m_Current + 1 >= m_Source.length()) return '\0';
    return m_Source[m_Current + 1];
}

bool Scanner::Match(char expected)
{
    if (IsAtEnd()) return false;
    if (m_Source[m_Current] != expected) return false;
    m_Current++;
    return true;
}

bool Scanner::IsAlpha(char c) const
{
    return std::isalpha(c) || c == '_';
}

void Scanner::String(char quotMark)
{
    bool escaped = false;
    while (!IsAtEnd() && (Peek() != quotMark || escaped))
    {
        if (Peek() == '\n') m_Line++;
        escaped = Advance() == '\\' && !escaped;
    }
    if (IsAtEnd())
    {
        AddErrorToken("Unterminated string");
        return;
    }
    Advance();

    AddToken(TokenType::String);
}

void Scanner::Number()
{
    while(std::isdigit(Peek())) Advance();
    // check for fractional part
    if (Peek() == '.' && std::isdigit(PeekNext()))
    {
        Advance();
        while(std::isdigit(Peek())) Advance();
    }
    AddToken(TokenType::Number);
}

void Scanner::Identifier()
{
    while(IsAlpha(Peek()) || std::isdigit(Peek())) Advance();

    std::string_view lexeme = m_Source.substr(m_Start, m_Current - m_Start);
    switch (lexeme[0])
    {
    case 'a': CheckKeyword(lexeme, "and", TokenType::And); return;
    case 'c': CheckKeyword(lexeme, "class", TokenType::Class); return;
    case 'e': CheckKeyword(lexeme, "else", TokenType::Else); return;
    case 'f': 
        if (m_Current - m_Start > 1)
        {
            switch (lexeme[1])
            {
            case 'a': CheckKeyword(lexeme, "false", TokenType::False); return;
            case 'o': CheckKeyword(lexeme, "for", TokenType::For); return;
            case 'u': CheckKeyword(lexeme, "fun", TokenType::Fun); return;
            default: break;
            }
        }
        break;
    case 'i': CheckKeyword(lexeme, "if", TokenType::If); return;
    case 'n': CheckKeyword(lexeme, "nil", TokenType::Nil); return;
    case 'o': CheckKeyword(lexeme, "or", TokenType::Or); return;
    case 'r': CheckKeyword(lexeme, "return", TokenType::Return); return;
    case 's': CheckKeyword(lexeme, "super", TokenType::Super); return;
    case 't':
        if (m_Current - m_Start > 1)
        {
            switch (lexeme[1])
            {
            case 'r': CheckKeyword(lexeme, "true", TokenType::True); return;
            case 'h': CheckKeyword(lexeme, "this", TokenType::This); return;
            default: break;
            }
        }
        break;
    case 'l': CheckKeyword(lexeme, "let", TokenType::Let); return;
    case 'w': CheckKeyword(lexeme, "while", TokenType::While); return;
    default: break;
    }
    AddToken(TokenType::Identifier);
}

void Scanner::CheckKeyword(std::string_view identifier, std::string_view keyword, TokenType match)
{
    if (identifier == keyword) AddToken(match);
    else AddToken(TokenType::Identifier);
}

void Scanner::ConsumeComment()
{
    while (!IsAtEnd() && Peek() != '\n') Advance();
}

void Scanner::ConsumeBlockComment()
{
    u32 openComments = 1;
    while (!IsAtEnd() && openComments > 0)
    {
        bool commentBegin = Match('/') && Peek() == '*'; 
        bool commentEnd = Match('*') && Peek() == '/';
        if (Peek() == '\n') m_Line++;
        openComments = openComments + u32(commentBegin) - u32(commentEnd);
        Advance();
    }
    if (openComments > 0)
    {
        AddErrorToken("Unterminated block comment");
    }
}
