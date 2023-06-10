#pragma once

#include "Chunk.h"
#include "Token.h"

#include <array>

class Compiler;

class CompilerResult
{
    friend class Compiler;
public:
    bool IsOk() const { return Ok; }
    const Chunk& Get() const { return Chunk; }
    Chunk& Get() { return Chunk; }
private:
    CompilerResult(bool ok, Chunk chunk)
        : Ok(ok), Chunk(std::move(chunk)) {}
private:
    bool Ok;
    Chunk Chunk;
};


struct Precedence
{
    // todo: comma
    enum Order
    {
        None = 0x00,
        Assignment = 0x01,
        Or = 0x02,
        And = 0x03,
        Equals = 0x04,
        Comparison = 0x05,
        Term = 0x06,
        Factor = 0x07,
        Unary = 0x08,
        Call = 0x09,
        Primary = 0x0a,
    };
};

struct ParseRule
{
    using CompilerParseFn = void (Compiler::*)();
    CompilerParseFn Prefix{nullptr};
    CompilerParseFn Infix{nullptr};
    Precedence::Order InfixPrecedence{Precedence::None};
};

class Compiler
{
    friend struct ParseRule;
public:
    Compiler();
    CompilerResult Compile(std::string_view source);
private:
    void Init();
    
    bool IsAtEnd() const;
    const Token& Advance();
    const Token& Peek() const;
    const Token& Previous() const;
    void Consume(TokenType type, std::string_view message);

    void Expression();
    void Grouping();
    void Binary();
    void Unary();
    void Number();

    void ParsePrecedence(Precedence::Order precedence);
    
    void PrintParseErrors();
    void ErrorAt(const Token& token, std::string_view message);
    void ErrorAtCurrent(std::string_view message);
    void Error(std::string_view message);

    void EmitByte(u8 byte) const;
    void EmitOperation(OpCode opCode) const;
    void EmitConstant(Value val) const;
    void EmitReturn() const;

    void OnCompileEnd();

    const ParseRule& GetRule(TokenType tokenType) const;
private:
    bool m_HadError{false};
    bool m_IsInPanic{false};

    std::vector<Token> m_Tokens;
    u32 m_CurrentTokenNum{0};

    Chunk* m_CurrentChunk{nullptr};
    std::array<ParseRule, static_cast<u32>(TokenType::Error) + 1> m_ParseRules;
};
