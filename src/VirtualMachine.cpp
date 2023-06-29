#include "VirtualMachine.h"

#include <format>
#include <iostream>
#include <fstream>
#include <ranges>

#include "Compiler.h"
#include "Core.h"
#include "Scanner.h"
#include "ValueFormatter.h"

namespace
{
    template<class... Ts> struct Overload : Ts... { using Ts::operator()...; };
}

#define BINARY_OP(stack, op)  \
    { \
        Value b = (stack).back(); (stack).pop_back(); \
        Value a = (stack).back(); (stack).pop_back(); \
        auto opResult = std::visit(Overload{ \
            [this](f64 a, f64 b) { (stack).push_back(a op b); return true; }, \
            [this](u64 a, u64 b) { (stack).push_back(a op b); return true; }, \
            [this](f64 a, u64 b) { (stack).push_back(a op (f64)b); return true; }, \
            [this](u64 a, f64 b) { (stack).push_back((f64)a op b); return true; }, \
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
    CallFrame frame;
    
    m_ValueStack.push_back(compilerResult.Get());
    m_CallFrames.push_back({.Fun = &compilerResult.Get().As<FunObj>(), .Ip = compilerResult.Get().As<FunObj>().Chunk.m_Code.data(), .Slot = 0});
    
    return Run();
}

InterpretResult VirtualMachine::Run()
{
    CallFrame* frame = &m_CallFrames.back();
    for(;;)
    {
#ifdef DEBUG_TRACE
        std::cout << "Stack trace: ";
        for (auto& v : m_ValueStack) { std::cout << std::format("[{}] ", v); }
        std::cout << "\n";
        Disassembler::DisassembleInstruction(frame->Fun->Chunk, (u32)(frame->Ip - frame->Fun->Chunk.m_Code.data()));
#endif
        auto instruction = static_cast<OpCode>(*frame->Ip++);
        switch (instruction)
        {
        case OpCode::OpConstant:
            m_ValueStack.push_back(ReadConstant());
            break;
        case OpCode::OpConstant32:
            m_ValueStack.push_back(ReadLongConstant());
            break;
        case OpCode::OpNil:
            m_ValueStack.emplace_back((void*)nullptr);
            break;
        case OpCode::OpFalse:
            m_ValueStack.emplace_back(false);
            break;
        case OpCode::OpTrue:
            m_ValueStack.emplace_back(true);
            break;
        case OpCode::OpNegate:
            {
                if (CheckOperandType<f64>(m_ValueStack.back())) std::get<f64>(m_ValueStack.back()) *= -1;
                else
                {
                    RuntimeError("Expected number.");
                    return InterpretResult::RuntimeError;
                }
                break;
            }
        case OpCode::OpNot:
            m_ValueStack.back() = IsFalsey(m_ValueStack.back());
            break;
        case OpCode::OpAdd:
            {
                Value b = m_ValueStack.back(); m_ValueStack.pop_back();
                Value a = m_ValueStack.back(); m_ValueStack.pop_back();
                auto sumResult = std::visit(Overload{
                    [this](f64 a, f64 b){ m_ValueStack.push_back(a + b); return true; },
                    [this](u64 a, u64 b){ m_ValueStack.push_back(a + b); return true; },
                    [this](f64 a, u64 b){ m_ValueStack.push_back(a + (f64)b); return true; },
                    [this](u64 a, f64 b){ m_ValueStack.push_back((f64)a + b); return true; },
                    [this](ObjHandle a, ObjHandle b)
                    {
                        if (a.HasType<StringObj>() && b.HasType<StringObj>())
                        {
                            m_ValueStack.emplace_back(AddString(a.As<StringObj>().String + b.As<StringObj>().String));
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
                Value a = m_ValueStack.back(); m_ValueStack.pop_back();
                Value b = m_ValueStack.back(); m_ValueStack.pop_back();
                m_ValueStack.emplace_back(AreEqual(a, b));
                break;
            }
        case OpCode::OpLess:
            BINARY_OP(m_ValueStack, <) break;
        case OpCode::OpLequal:
            BINARY_OP(m_ValueStack, <=) break;
        case OpCode::OpPrint:
            PrintValue(m_ValueStack.back());
            m_ValueStack.pop_back();
            break;
        case OpCode::OpPop:
            m_ValueStack.pop_back();
            break;
        case OpCode::OpPopN:
            {
                u32 count = (u32)std::get<u64>(m_ValueStack.back()); m_ValueStack.pop_back();
                m_ValueStack.erase(m_ValueStack.end() - count, m_ValueStack.end());
                break;
            }
        case OpCode::OpDefineGlobal:
            {
                ObjHandle varName = std::get<ObjHandle>(ReadConstant());
                m_Globals[varName] = m_ValueStack.back(); m_ValueStack.pop_back();
                break;
            }
        case OpCode::OpDefineGlobal32:
            {
                ObjHandle varName = std::get<ObjHandle>(ReadLongConstant());
                m_Globals[varName] = m_ValueStack.back(); m_ValueStack.pop_back();
                break;
            }  
        case OpCode::OpReadGlobal:
            {
                ObjHandle varName = std::get<ObjHandle>(ReadConstant());
                if (!m_Globals.contains(varName))
                {
                    RuntimeError(std::format("Variable \"{}\" is not defined", varName.As<StringObj>().String));
                    return InterpretResult::RuntimeError;
                }
                m_ValueStack.push_back(m_Globals.at(varName));
                break;
            }
        case OpCode::OpReadGlobal32:
            {
                ObjHandle varName = std::get<ObjHandle>(ReadLongConstant());
                if (!m_Globals.contains(varName))
                {
                    RuntimeError(std::format("Variable \"{}\" is not defined", varName.As<StringObj>().String));
                    return InterpretResult::RuntimeError;
                }
                m_ValueStack.push_back(m_Globals.at(varName));
                break;
            }
        case OpCode::OpSetGlobal:
            {
                ObjHandle varName = std::get<ObjHandle>(ReadConstant());
                if (!m_Globals.contains(varName))
                {
                    RuntimeError(std::format("Variable \"{}\" is not defined", varName.As<StringObj>().String));
                    return InterpretResult::RuntimeError;
                }
                m_Globals.at(varName) = m_ValueStack.back();
                break;
            }
        case OpCode::OpSetGlobal32:
            {
                ObjHandle varName = std::get<ObjHandle>(ReadLongConstant());
                if (!m_Globals.contains(varName))
                {
                    RuntimeError(std::format("Variable \"{}\" is not defined", varName.As<StringObj>().String));
                    return InterpretResult::RuntimeError;
                }
                m_Globals.at(varName) = m_ValueStack.back();
                break;
            }
        case OpCode::OpReadLocal:
            {
                u32 varIndex = ReadByte();
                m_ValueStack.push_back(m_ValueStack[frame->Slot + varIndex]);
                break;
            }
        case OpCode::OpReadLocal32:
            {
                u32 varIndex = ReadU32();
                m_ValueStack.push_back(m_ValueStack[frame->Slot + varIndex]);
                break;
            }
        case OpCode::OpSetLocal:
            {
                u32 varIndex = ReadByte();
                m_ValueStack[frame->Slot + varIndex] = m_ValueStack.back();
                break;
            }
        case OpCode::OpSetLocal32:
            {
                u32 varIndex = ReadU32();
                m_ValueStack[frame->Slot + varIndex] = m_ValueStack.back();
                break;
            }
        case OpCode::OpJump:
            {
                i32 jump = ReadI32();
                frame->Ip += jump;
                break;
            }
        case OpCode::OpJumpFalse:
            {
                i32 jump = ReadI32();
                if (IsFalsey(m_ValueStack.back())) frame->Ip += jump;
                break;
            }
        case OpCode::OpJumpTrue:
            {
                i32 jump = ReadI32();
                if (!IsFalsey(m_ValueStack.back())) frame->Ip += jump;
                break;
            }
        case OpCode::OpCall:
            {
                u8 argc = ReadByte();
                if (!CallValue(m_ValueStack[m_ValueStack.size() - 1 - argc], argc))
                {
                    return InterpretResult::RuntimeError;
                }
                frame = &m_CallFrames.back();
                break;
            }
        case OpCode::OpReturn:
            {
                Value funRes = m_ValueStack.back(); m_ValueStack.pop_back();
                u32 frameSlot = frame->Slot;
                m_CallFrames.pop_back();
                if (m_CallFrames.empty())
                {
                    m_ValueStack.pop_back(); // pop <script> name
                    return InterpretResult::Ok;
                }
                m_ValueStack.erase(m_ValueStack.begin() + frameSlot, m_ValueStack.end());
                m_ValueStack.push_back(funRes);
                frame = &m_CallFrames.back();
                break;
            }
        }
    }
}

bool VirtualMachine::CallValue(Value callee, u8 argc)
{
    if (std::holds_alternative<ObjHandle>(callee))
    {
        ObjHandle obj = std::get<ObjHandle>(callee);
        switch (obj.GetType())
        { 
        case ObjType::Fun: return Call(obj.As<FunObj>(), argc);
        }
    }
    RuntimeError("Can only call functions and classes.");
    return false;
}

bool VirtualMachine::Call(FunObj& fun, u8 argc)
{
    if (fun.Arity != argc)
    {
        RuntimeError(std::format("Expected {} arguments, but got {}.", fun.Arity, argc));
        return false;
    }
    CallFrame callFrame;
    callFrame.Fun = &fun;
    callFrame.Ip = fun.Chunk.m_Code.data();
    callFrame.Slot = m_ValueStack.size() - 1 - argc;
    m_CallFrames.push_back(callFrame);
    return true;
}

OpCode VirtualMachine::ReadInstruction()
{
    CallFrame& frame = m_CallFrames.back();
    return static_cast<OpCode>(*frame.Ip++);
}

Value VirtualMachine::ReadConstant()
{
    CallFrame& frame = m_CallFrames.back();
    return frame.Fun->Chunk.m_Values[ReadByte()];
}

Value VirtualMachine::ReadLongConstant()
{
    CallFrame& frame = m_CallFrames.back();
    return frame.Fun->Chunk.m_Values[ReadU32()];
}

u8 VirtualMachine::ReadByte()
{
    CallFrame& frame = m_CallFrames.back();
    return *frame.Ip++;
}

i32 VirtualMachine::ReadI32()
{
    CallFrame& frame = m_CallFrames.back();
    i32 index = *reinterpret_cast<i32*>(&frame.Ip[0]);
    frame.Ip += 4;
    return index;
}

u32 VirtualMachine::ReadU32()
{
    CallFrame& frame = m_CallFrames.back();
    u32 index = *reinterpret_cast<u32*>(&frame.Ip[0]);
    frame.Ip += 4;
    return index;
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
    m_ValueStack.clear();
}

void VirtualMachine::RuntimeError(const std::string& message)
{
    std::string errorMessage = std::format("{}\n", message);
    for (auto& m_CallFrame : std::ranges::reverse_view(m_CallFrames))
    {
        u32 instruction = (u32)(m_CallFrame.Ip - m_CallFrame.Fun->Chunk.m_Code.data()) - 1;
        u32 line = m_CallFrame.Fun->Chunk.GetLine(instruction);
        errorMessage += std::format("[line {}] in {}()\n", line, m_CallFrame.Fun->GetName());
    }
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
        [](u64 a, u64 b) { return a == b; },
        [](f64 a, u64 b) { return a == (f64)b; },
        [](u64 a, f64 b) { return (f64)a == b; },
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
