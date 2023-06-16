﻿#pragma once

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
    using CompilerParseFn = void (Compiler::*)(bool);
    CompilerParseFn Prefix{nullptr};
    CompilerParseFn Infix{nullptr};
    Precedence::Order InfixPrecedence{Precedence::None};
};

class Compiler
{
    friend struct ParseRule;
public:
    Compiler(VirtualMachine* vm);
    CompilerResult Compile(std::string_view source);
private:
    void Init();
    
    bool IsAtEnd() const;
    template <typename ...Types>
    bool Match(Types&& ... types);
    bool Check(TokenType type) const;
    const Token& Advance();
    const Token& Peek() const;
    const Token& Previous() const;
    void Consume(TokenType type, std::string_view message);

    void Synchronize();
    
    void Declaration();
    void VarDeclaration();
    void Statement();
    void PrintStatement();
    void ExpressionStatement();
    
    void Expression();
    void Grouping([[maybe_unused]] bool canAssign);
    void Binary([[maybe_unused]] bool canAssign);
    void Unary([[maybe_unused]] bool canAssign);
    void Number([[maybe_unused]] bool canAssign);
    void String([[maybe_unused]] bool canAssign);
    void Variable([[maybe_unused]] bool canAssign);
    void Nil([[maybe_unused]] bool canAssign);
    void False([[maybe_unused]] bool canAssign);
    void True([[maybe_unused]] bool canAssign);

    void ParsePrecedence(Precedence::Order precedence);
    u32 ParseVariable(std::string_view message);
    void DefineVariable(u32 variableName);
    u32 NamedVariable(const Token& name);
    u32 AddOrGetGlobalIndex(ObjHandle variableName);
    
    void PrintParseErrors();
    void ErrorAt(const Token& token, std::string_view message);
    void ErrorAtCurrent(std::string_view message);
    void Error(std::string_view message);

    void EmitByte(u8 byte);
    void EmitOperation(OpCode opCode);
    u32 EmitConstantCode(Value val);
    u32 EmitConstantVal(Value val);
    void EmitReturn();

    void OnCompileEnd();

    const ParseRule& GetRule(TokenType tokenType) const;
private:
    VirtualMachine* m_VirtualMachine;
    
    bool m_HadError{false};
    bool m_IsInPanic{false};

    std::vector<Token> m_Tokens;
    u32 m_CurrentTokenNum{0};

    Chunk* m_CurrentChunk{nullptr};
    std::array<ParseRule, static_cast<u32>(TokenType::Error) + 1> m_ParseRules;
};

template <typename ... Types>
bool Compiler::Match(Types&&... types)
{
    bool match = (Check(types), ...);
    if (match) Advance();
    return match;
}
