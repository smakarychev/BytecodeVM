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
    void Compile(std::string_view source);
    InterpretResult ProcessChunk(Chunk* chunk);
    OpCode ReadInstruction();
    Value ReadConstant();
    Value ReadLongConstant();
private:
    Chunk* m_Chunk{nullptr};
    u8* m_Ip{nullptr};
    std::stack<Value> m_ValueStack;
    
};
