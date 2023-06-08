#include "VirtualMachine.h"

#include <format>
#include <iostream>
#include <fstream>

#include "Core.h"
#include "Scanner.h"
#include "Scanner.h"

#define BINARY_OP(stack, op)  \
    { \
    f64 b = (stack).top(); (stack).pop(); \
    f64 a = (stack).top(); (stack).pop(); \
    (stack).push(a op b); \
    }

VirtualMachine::VirtualMachine()
{
    Init();
}

void VirtualMachine::Init()
{
    m_ValueStack = std::stack<Value>{};
}

void VirtualMachine::Repl()
{
    for (;;)
    {
        std::cout << "> ";
        std::string promptLine{};
        std::getline(std::cin, promptLine);
        Interpret(promptLine);
    }
}

void VirtualMachine::RunFile(std::string_view path)
{
    std::ifstream in(path.data(), std::ios::in | std::ios::binary);
    CHECK_RETURN(in, "Failed to read file {}.", path)
    std::string source{(std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>()};
    
    InterpretResult result = Interpret(source);
    if (result == InterpretResult::CompileError) exit(65);
    if (result == InterpretResult::RuntimeError) exit(70);
}

InterpretResult VirtualMachine::Interpret(std::string_view source)
{
    Compile(source);
    return InterpretResult::Ok;
}

void VirtualMachine::Compile(std::string_view source)
{
    // scanning
    Scanner scanner(source);
    std::vector<Token> tokens = scanner.ScanTokens();
    for (auto& t : tokens) std::cout << t << "\n";
}

InterpretResult VirtualMachine::ProcessChunk(Chunk* chunk)
{
    m_Chunk = chunk;
    m_Ip = m_Chunk->m_Code.data();
    for(;;)
    {
#ifdef DEBUG_TRACE
        std::cout << "Stack trace: ";
        for (auto& v : m_ValueStack._Get_container()) { std::cout << std::format("[{}] ", v); }
        std::cout << "\n";
        Disassembler::DisassembleInstruction(*chunk, (u32)(m_Ip - chunk->m_Code.data()));
#endif
        auto instruction = static_cast<OpCode>(*m_Ip++);
        switch (instruction)
        {
        case OpCode::OpConstant:
            m_ValueStack.push(ReadConstant());
            break;
        case OpCode::OpConstantLong:
            m_ValueStack.push(ReadLongConstant());
            break;
        case OpCode::OpNegate:
            {
                m_ValueStack.top() *= -1;
                break;
            }
        case OpCode::OpAdd:
            BINARY_OP(m_ValueStack, +) break;
        case OpCode::OpSubtract: 
            BINARY_OP(m_ValueStack, -) break;
        case OpCode::OpMultiply: 
            BINARY_OP(m_ValueStack, *) break;
        case OpCode::OpDivide: 
            BINARY_OP(m_ValueStack, /) break;
        case OpCode::OpReturn:
            return InterpretResult::Ok;
        }
    }
}

OpCode VirtualMachine::ReadInstruction()
{
    return static_cast<OpCode>(*m_Ip++);
}

Value VirtualMachine::ReadConstant()
{
    return m_Chunk->m_Values[*m_Ip++];
}

Value VirtualMachine::ReadLongConstant()
{
    u8 shift = Chunk::BYTE_SHIFT;
    u32 index = m_Ip[0] | (m_Ip[1] << 1 * shift) | (m_Ip[2] << 2 * shift);
    m_Ip+=3;
    return m_Chunk->m_Values[index];
}

#undef BINARY_OP