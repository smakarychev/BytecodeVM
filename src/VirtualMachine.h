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
    InterpretResult Interpret(Chunk* chunk);
private:
    OpCode ReadInstruction();
    Value ReadConstant();
    Value ReadLongConstant();
private:
    Chunk* m_Chunk{nullptr};
    u8* m_Ip{nullptr};
    std::stack<Value> m_ValueStack;
};
