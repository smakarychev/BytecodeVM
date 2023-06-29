#pragma once
#include <stack>
#include <unordered_map>

#include "Chunk.h"
#include "Obj.h"
#include "Value.h"

class Chunk;

enum class InterpretResult { Ok, CompileError, RuntimeError };

struct CallFrame
{
    FunObj* Fun{nullptr};
    u8* Ip{nullptr};
    u32 Slot{0};
};

class VirtualMachine
{
    friend class Compiler;
public:
    VirtualMachine();
    ~VirtualMachine();
    void Init();
    void Repl();
    void RunFile(std::string_view path);
    InterpretResult Interpret(std::string_view source);
private:
    InterpretResult Run();
    bool CallValue(Value callee, u8 argc);
    bool Call(FunObj& fun, u8 argc);
    
    OpCode ReadInstruction();
    Value ReadConstant();
    Value ReadLongConstant();
    u8 ReadByte();
    i32 ReadI32();
    u32 ReadU32();
    void PrintValue(Value val);

    ObjHandle AddString(const std::string& val);
    
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
    std::vector<CallFrame> m_CallFrames;
    std::vector<Value> m_ValueStack;
    std::unordered_map<std::string, ObjHandle> m_InternedStrings;
    std::unordered_map<ObjHandle, Value> m_Globals;
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
        std::get<ObjHandle>(operand).HasType<Type>();
    return checked;
}

template <typename Type, typename ... Operands>
bool VirtualMachine::CheckOperandsTypeObj(Operands... operands)
{
    bool checked =
        CheckOperandsType<ObjHandle>(std::forward<Operands>(operands)...) &&
        !(!std::get<ObjHandle>(operands).template HasType<Type>() || ...);
    return checked;
}