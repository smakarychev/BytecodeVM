#include "VirtualMachine.h"

#include <format>
#include <iostream>
#include <fstream>

#include "Compiler.h"
#include "Core.h"
#include "Scanner.h"

namespace
{
    template<class... Ts> struct Overload : Ts... { using Ts::operator()...; };
}

#define BINARY_OP(stack, op)  \
    { \
    Value b = (stack).top(); (stack).pop(); \
    Value a = (stack).top(); (stack).pop(); \
    if (!CheckOperandsType<f64>("Expected numbers.", a, b)) return InterpretResult::RuntimeError; \
    (stack).push(std::get<f64>(a) op std::get<f64>(b)); \
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
    Compiler compiler;
    CompilerResult compilerResult = compiler.Compile(source);
    if (!compilerResult.IsOk()) return InterpretResult::CompileError;
    Chunk& chunk = compilerResult.Get();
    return ProcessChunk(&chunk);
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
        case OpCode::OpConstant24:
            m_ValueStack.push(ReadLongConstant());
            break;
        case OpCode::OpNil:
            m_ValueStack.push(nullptr);
            break;
        case OpCode::OpFalse:
            m_ValueStack.push(false);
            break;
        case OpCode::OpTrue:
            m_ValueStack.push(true);
            break;
        case OpCode::OpNegate:
            {
                if (!CheckOperandType<f64>("Expected number.", m_ValueStack.top())) return InterpretResult::RuntimeError;
                std::get<f64>(m_ValueStack.top()) *= -1;
                break;
            }
        case OpCode::OpNot:
            m_ValueStack.top() = IsFalsey(m_ValueStack.top());
            break;
        case OpCode::OpAdd:
            BINARY_OP(m_ValueStack, +) break;
        case OpCode::OpSubtract: 
            BINARY_OP(m_ValueStack, -) break;
        case OpCode::OpMultiply: 
            BINARY_OP(m_ValueStack, *) break;
        case OpCode::OpDivide: 
            BINARY_OP(m_ValueStack, /) break;
        case OpCode::OpEqual:
            {
                Value a = m_ValueStack.top(); m_ValueStack.pop();
                Value b = m_ValueStack.top(); m_ValueStack.pop();
                m_ValueStack.push(AreEqual(a, b));
                break;
            }
        case OpCode::OpLess:
            BINARY_OP(m_ValueStack, <) break;
        case OpCode::OpLequal:
            BINARY_OP(m_ValueStack, <=) break;
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

void VirtualMachine::RuntimeError(const std::string& message)
{
    u32 instruction = (u32)(m_Ip - m_Chunk->m_Code.data());
    u32 line = m_Chunk->GetLine(instruction);
    std::string errorMessage = std::format("[line {}] : {}", line, message);
    LOG_ERROR("Runtime: {}", errorMessage);
    // todo: clear stack?
}

bool VirtualMachine::IsFalsey(Value val) const
{
    return std::visit(Overload {
        [](bool b) { return !b; },
        [](void*) { return true; },
        [](auto) { return false; }
    }, val);
}

bool VirtualMachine::AreEqual(Value a, Value b) const
{
    return std::visit(Overload {
        [](f64 a, f64 b) { return a == b; },
        [](bool a, bool b) { return a == b; },
        [](void*) { return true; },
        [](auto, auto) { return false; }
    }, a, b);
}

#undef BINARY_OP
