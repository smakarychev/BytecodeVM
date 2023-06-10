#pragma once
#include <stack>

#include "Chunk.h"
#include "Value.h"

class Chunk;

enum class InterpretResult { Ok, CompileError, RuntimeError };

class VirtualMachine
{
public:
    VirtualMachine();
    void Init();
    void Repl();
    void RunFile(std::string_view path);
    InterpretResult Interpret(std::string_view source);
private:
    InterpretResult ProcessChunk(Chunk* chunk);
    OpCode ReadInstruction();
    Value ReadConstant();
    Value ReadLongConstant();

    void RuntimeError(const std::string& message);

    bool IsFalsey(Value val) const;
    bool AreEqual(Value a, Value b) const;
    template <typename ... Types>
    bool CheckOperandType(const std::string& message, Value operand);
    template <typename ... Types, typename ... Operands >
    bool CheckOperandsType(const std::string& message, Operands... operands);
private:
    Chunk* m_Chunk{nullptr};
    u8* m_Ip{nullptr};
    std::stack<Value> m_ValueStack;
    
};

template <typename ... Types>
bool VirtualMachine::CheckOperandType(const std::string& message, Value operand)
{
    bool checked = std::holds_alternative<Types...>(operand);
    if (!checked) RuntimeError(message);
    return checked;
}

template <typename ... Types, typename ... Operands>
bool VirtualMachine::CheckOperandsType(const std::string& message, Operands... operands)
{
    bool checked = !(!std::holds_alternative<Types...>(operands) || ...);
    if (!checked) RuntimeError(message);
    return checked;
}
