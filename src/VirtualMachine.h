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
    ~VirtualMachine();
    void Init();
    void Repl();
    void RunFile(std::string_view path);
    InterpretResult Interpret(std::string_view source);
private:
    InterpretResult ProcessChunk(Chunk* chunk);
    OpCode ReadInstruction();
    Value ReadConstant();
    Value ReadLongConstant();

    void ClearStack();

    void RuntimeError(const std::string& message);

    bool IsFalsey(Value val) const;
    bool AreEqual(Value a, Value b) const;
    template <typename ... Types>
    bool CheckOperandType(Value operand);
    template <typename ... Types, typename ... Operands >
    bool CheckOperandsType(Operands... operands);
    template <typename Type>
    bool CheckOperandTypeObj(Value operand);
    template <typename Type, typename ... Operands >
    bool CheckOperandsTypeObj(Operands... operands);
private:
    Chunk* m_Chunk{nullptr};
    u8* m_Ip{nullptr};
    std::stack<Value> m_ValueStack;
    
};

template <typename ... Types>
bool VirtualMachine::CheckOperandType(Value operand)
{
    bool checked = std::holds_alternative<Types...>(operand);
    return checked;
}

template <typename ... Types, typename ... Operands>
bool VirtualMachine::CheckOperandsType(Operands... operands)
{
    bool checked = !(!std::holds_alternative<Types...>(operands) || ...);
    return checked;
}

template <typename Type>
bool VirtualMachine::CheckOperandTypeObj(Value operand)
{
    bool checked =
        std::holds_alternative<ObjHandle>(operand) &&
        ObjRegistry::HasType<Type>(std::get<ObjHandle>(operand));
}

template <typename Type, typename ... Operands>
bool VirtualMachine::CheckOperandsTypeObj(Operands... operands)
{
    bool checked =
        CheckOperandsType<ObjHandle>(std::forward<Operands>(operands)...) &&
        !(!ObjRegistry::HasType<Type>(std::get<ObjHandle>(operands)) || ...);
    return checked;
}