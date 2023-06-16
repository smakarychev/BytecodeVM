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
        auto opResult = std::visit(Overload{ \
            [this](f64 a, f64 b) { (stack).push(a op b); return true; }, \
            [this](u64 a, u64 b) { (stack).push(a op b); return true; }, \
            [this](auto, auto) { return false; }, \
        }, a, b); \
        if (!opResult) \
        { \
            RuntimeError("Expected numbers."); \
            return InterpretResult::RuntimeError; \
        } \
    }

VirtualMachine::VirtualMachine()
{
    Init();
}

VirtualMachine::~VirtualMachine()
{
    m_InternedStrings.clear();
    ObjRegistry::Shutdown();
}

void VirtualMachine::Init()
{
    ClearStack();
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
    Compiler compiler(this);
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
            m_ValueStack.push((void*)nullptr);
            break;
        case OpCode::OpFalse:
            m_ValueStack.push(false);
            break;
        case OpCode::OpTrue:
            m_ValueStack.push(true);
            break;
        case OpCode::OpNegate:
            {
                if (CheckOperandType<f64>(m_ValueStack.top())) std::get<f64>(m_ValueStack.top()) *= -1;
                else
                {
                    RuntimeError("Expected number.");
                    return InterpretResult::RuntimeError;
                }
                break;
            }
        case OpCode::OpNot:
            m_ValueStack.top() = IsFalsey(m_ValueStack.top());
            break;
        case OpCode::OpAdd:
            {
                Value b = (m_ValueStack).top(); (m_ValueStack).pop();
                Value a = (m_ValueStack).top(); (m_ValueStack).pop();
                auto sumResult = std::visit(Overload{
                    [this](f64 a, f64 b){ m_ValueStack.push(a + b); return true; },
                    [this](u64 a, u64 b){ m_ValueStack.push(a + b); return true; },
                    [this](ObjHandle a, ObjHandle b)
                    {
                        if (a.HasType<StringObj>() && b.HasType<StringObj>())
                        {
                            m_ValueStack.push(AddString(a.As<StringObj>().String + b.As<StringObj>().String));
                            return true;
                        }
                        return false;
                    },
                    [](auto, auto) { return false; }
                }, a, b);
                if (!sumResult)
                {
                    RuntimeError("Expected strings or numbers."); return InterpretResult::RuntimeError;
                }
            break;
            }
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
        case OpCode::OpPrint:
            PrintValue(m_ValueStack.top());
            m_ValueStack.pop();
            break;
        case OpCode::OpPop:
            m_ValueStack.pop();
            break;
        case OpCode::OpDefineGlobal:
            {
                u32 varIndex = (u32)std::get<u64>(m_ValueStack.top()); m_ValueStack.pop();
                ObjHandle varName = std::get<ObjHandle>(m_Chunk->m_Values[varIndex]);
                m_Globals[varName] = m_ValueStack.top(); m_ValueStack.pop();
                break;
            }   
        case OpCode::OpReadGlobal:
            {
                u32 varIndex = (u32)std::get<u64>(m_ValueStack.top()); m_ValueStack.pop();
                ObjHandle varName = std::get<ObjHandle>(m_Chunk->m_Values[varIndex]);
                if (!m_Globals.contains(varName))
                {
                    RuntimeError(std::format("Variable \"{}\" is not defined", varName.As<StringObj>().String));
                    return InterpretResult::RuntimeError;
                }
                m_ValueStack.push(m_Globals.at(varName));
                break;
            }
        case OpCode::OpSetGlobal:
            {
                u32 varIndex = (u32)std::get<u64>(m_ValueStack.top()); m_ValueStack.pop();
                ObjHandle varName = std::get<ObjHandle>(m_Chunk->m_Values[varIndex]);
                if (!m_Globals.contains(varName))
                {
                    RuntimeError(std::format("Variable \"{}\" is not defined", varName.As<StringObj>().String));
                    return InterpretResult::RuntimeError;
                }
                m_Globals.at(varName) = m_ValueStack.top();
                break;
            }
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

void VirtualMachine::PrintValue(Value val)
{
    std::cout << std::format("{}\n", val);
}

ObjHandle VirtualMachine::AddString(const std::string& val)
{
    if (m_InternedStrings.contains(val)) return m_InternedStrings.at(val);
    ObjHandle newString = ObjRegistry::CreateObj<StringObj>(val);
    m_InternedStrings.emplace(val, newString);
    return newString;
}

void VirtualMachine::ClearStack()
{
    m_ValueStack = std::stack<Value>{};
}

void VirtualMachine::RuntimeError(const std::string& message)
{
    u32 instruction = (u32)(m_Ip - m_Chunk->m_Code.data());
    u32 line = m_Chunk->GetLine(instruction);
    std::string errorMessage = std::format("[line {}] : {}", line, message);
    LOG_ERROR("Runtime: {}", errorMessage);
    ClearStack();
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
    using objCompFn = bool (*)(ObjHandle, ObjHandle);
    objCompFn objComparisons[(u32)ObjType::Count][(u32)ObjType::Count] = {{nullptr}};
    objComparisons[(u32)ObjType::String][(u32)ObjType::String] = [](ObjHandle strA, ObjHandle strB)
    {
        return strA == strB;
    };
    return std::visit(Overload {
        [](f64 a, f64 b) { return a == b; },
        [](bool a, bool b) { return a == b; },
        [](void*, void*) { return true; },
        [&objComparisons](ObjHandle a, ObjHandle b)
        {
              return objComparisons[(u32)a.GetType()][(u32)b.GetType()](a, b);
        },
        [](auto, auto) { return false; }
    }, a, b);
}

#undef BINARY_OP
