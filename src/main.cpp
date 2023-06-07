#include "Chunk.h"

int main()
{
    Chunk chunk{"Test Chunk"};
    for (u32 i = 0; i < 300; i++)
    {
        chunk.AddConstant(0.14, i);
    }
    chunk.AddOperation(OpCode::OpReturn, 123);
    chunk.AddOperation(OpCode::OpReturn, 124);
    chunk.AddOperation(OpCode::OpReturn, 125);
    chunk.AddOperation(OpCode::OpReturn, 125);
    chunk.AddOperation(OpCode::OpReturn, 125);
    chunk.AddOperation(OpCode::OpReturn, 125);
    chunk.AddOperation(OpCode::OpReturn, 125);
    Disassembler disassembler;
    disassembler.Disassemble(chunk);
    return 0;
}