#include "Chunk.h"
#include "Log.h"
#include "VirtualMachine.h"
int main(u32 argc, char** argv)
{
    VirtualMachine virtualMachine{};
    if (argc > 2)
    {
        LOG_ERROR("Incorrect number of arguments.");
        LOG_INFO("Usage: BytecodeVM [script_file].");
    }
    else if (argc == 2)
    {
        LOG_INFO("Running in file mode. File: {}.", argv[1]);
        virtualMachine.RunFile(argv[1]);
    }
    if (argc == 1)
    {
        LOG_INFO("Running in prompt mode.");
        virtualMachine.Repl();
    }
    return 0;
}
