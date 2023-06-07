#include "Chunk.h"
#include "VirtualMachine.h"

int main()
{
    Chunk chunk{"Test Chunk"};
    chunk.AddConstant(0.14, 0);
    chunk.AddOperation(OpCode::OpReturn, 1);
    VirtualMachine virtualMachine{};
    virtualMachine.Interpret(&chunk);
    return 0;
}
