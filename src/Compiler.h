#pragma once

#include "Chunk.h"
#include "Token.h"
#include "Obj.h"

#include <array>

class Compiler;

class CompilerResult
{
    friend class Compiler;
public:
    bool IsOk() const { return Ok; }
    ObjHandle& Get() { return Fun; }
private:
    CompilerResult(bool ok, ObjHandle fun)
        : Ok(ok), Fun(std::move(fun)) {}
private:
    bool Ok;
    ObjHandle Fun;
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

struct LocalVar
{
    Token Name;
    u32 Depth;
    bool IsCaptured{false};
    // depth that a variable gets, when it declared but not defined yet
    static constexpr u32 DEPTH_UNDEFINED = std::numeric_limits<u32>::max();
    static constexpr u32 INVALID_INDEX = std::numeric_limits<u32>::max();
};

struct UpvalueVar
{
    u8 Index{INVALID_INDEX};
    bool IsLocal{false};
    friend auto operator<=>(const UpvalueVar&, const UpvalueVar&) = default;
    static constexpr u8 INVALID_INDEX = std::numeric_limits<u8>::max();
};

enum class FunType{ Script, Function };

struct CompilerContext
{
    CompilerContext();
    CompilerContext(FunType funType);
    FunType FunType{FunType::Script};
    ObjHandle Fun{ObjHandle::NonHandle()};
    std::vector<LocalVar> LocalVars;
    std::vector<UpvalueVar> Upvalues;
    u32 ScopeDepth{0};
    CompilerContext* Previous{nullptr};
};

class Compiler
{
    friend struct ParseRule;
    friend class GarbageCollector;
public:
    Compiler(VirtualMachine* vm);
    void Init();
    CompilerResult Compile(std::string_view source);
private:
    void InitParseTable();
    void InitContext(FunType funType, std::string_view funName);
    Chunk& CurrentChunk();
    const Chunk& CurrentChunk() const;
    
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
    void FunDeclaration();
    void Statement();
    void Block();
    void IfStatement();
    void WhileStatement();
    void ForStatement();
    void PrintStatement();
    void ReturnStatement();
    void ExpressionStatement();

    void Function(FunType type);
    
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
    void And([[maybe_unused]] bool canAssign);
    void Or([[maybe_unused]] bool canAssign);
    void Call([[maybe_unused]] bool canAssign);

    void ParsePrecedence(Precedence::Order precedence);
    u32 ParseVariable(std::string_view message);
    void DefineVariable(u32 variableName);
    u32 NamedLocalVar(const Token& name);
    u8 NamedUpvalue(const Token& name);
    u32 NamedGlobalVar(const Token& name);
    u32 AddOrGetGlobalIndex(ObjHandle variableName);
    void MarkDefined();

    // local variables
    void PushScope();
    void PopScope();
    void PopLocals(u32 count);
    void AddLocal(const Token& name);
    u8 AddUpvalue(u8 index, bool isLocal);
    void DeclareVariable();
    
    void PrintParseErrors();
    void ErrorAt(const Token& token, std::string_view message);
    void ErrorAtCurrent(std::string_view message);
    void Error(std::string_view message);

    void EmitByte(u8 byte);
    void EmitOperation(OpCode opCode);
    void EmitOperation(OpCode opCode, u32 operandIndex);
    u32 EmitConstant(Value val);
    void EmitReturn();
    u32 EmitJump(OpCode jumpCode);
    void PatchJump(u32 jumpTo, u32 jumpFrom);
    
    void OnCompileEnd();
    void OnCompileSubFunctionEnd();

    const ParseRule& GetRule(TokenType tokenType) const;
private:
    VirtualMachine* m_VirtualMachine;
    std::array<ParseRule, static_cast<u32>(TokenType::Error) + 1> m_ParseRules;
    
    bool m_HadError{false};
    bool m_IsInPanic{false};
    bool m_NoEmit{false};
    OpCode m_LastEmittedOpcode{OpCode::OpPop};

    std::vector<Token> m_Tokens;
    u32 m_CurrentTokenNum{0};

    CompilerContext m_CurrentContext;
};

template <typename ... Types>
bool Compiler::Match(Types&&... types)
{
    bool match = (Check(types), ...);
    if (match) Advance();
    return match;
}
