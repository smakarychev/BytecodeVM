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

    void ClearStack();

    void RuntimeError(const std::string& message);

    bool IsFalsey(Value val) const;
    bool AreEqual(Value a, Value b) const;
    template <typename ... Types>
    bool CheckOperandType(Value operand);
    template <typename ... Types, typename ... Operands >
    bool CheckOperandsType(Operands... operands);
    bool CheckOperandTypeObj(ObjType type,  Value operand);
    template <typename ... Operands >
    bool CheckOperandsTypeObj(ObjType type, Operands... operands);
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

template <typename ... Operands>
bool VirtualMachine::CheckOperandsTypeObj(ObjType type, Operands... operands)
{
    bool checked =
        CheckOperandsType<Obj*>(std::forward<Operands>(operands)...) &&
        !(!(std::get<Obj*>(operands)->GetType() == type) || ...);
    return checked;
}
