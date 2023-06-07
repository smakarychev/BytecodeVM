#include "VirtualMachine.h"

#include <format>
#include <iostream>

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

InterpretResult VirtualMachine::Interpret(Chunk* chunk)
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