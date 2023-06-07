#include "Chunk.h"
#include "VirtualMachine.h"

int main()
{
    Chunk chunk{"Test Chunk"};
    chunk.AddConstant(0.14, 1);
    chunk.AddConstant(0.18, 1);
    chunk.AddOperation(OpCode::OpAdd, 1);
    chunk.AddOperation(OpCode::OpReturn, 2);
    VirtualMachine virtualMachine{};
    virtualMachine.Interpret(&chunk);
    return 0;
}
