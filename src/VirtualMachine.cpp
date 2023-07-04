#include "VirtualMachine.h"

#include <format>
#include <iostream>
#include <fstream>
#include <ranges>

#include "Compiler.h"
#include "Core.h"
#include "Scanner.h"
#include "ValueFormatter.h"

#define BINARY_OP(stack, op)  \
    { \
        /* it hurts me as much as it does you, but this is the fastest way */ \
        Value b = (stack).back(); (stack).pop_back(); \
        Value a = (stack).back(); (stack).pop_back(); \
        if (a.HasType<f64>()) \
        { \
            if (b.HasType<f64>()) (stack).emplace_back(a.As<f64>() op b.As<f64>()); \
            else if (b.HasType<u64>()) (stack).emplace_back(a.As<f64>() op (f64)b.As<u64>()); \
            else { RuntimeError("Expected numbers."); return InterpretResult::RuntimeError; } \
        } \
        else if (a.HasType<u64>()) \
        { \
            if (b.HasType<f64>()) (stack).emplace_back((f64)a.As<u64>() op b.As<f64>()); \
            else if (b.HasType<u64>()) (stack).emplace_back((f64)a.As<u64>() op (f64)b.As<u64>()); \
            else { RuntimeError("Expected numbers."); return InterpretResult::RuntimeError; } \
        } \
        else { RuntimeError("Expected numbers."); return InterpretResult::RuntimeError; } \
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
    InitNativeFunctions();
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
    
    m_ValueStack.push_back(compilerResult.Get());
    m_CallFrames.push_back({.Fun = &compilerResult.Get().As<FunObj>(), .Ip = compilerResult.Get().As<FunObj>().Chunk.m_Code.data(), .Slot = 0});
    
    return Run();
}

void VirtualMachine::InitNativeFunctions()
{
    auto clock = [](u8 argc, Value* argv)
    {
        BCVM_ASSERT(argc == 0, "'clock()' accepts 0 arguments, but {} given", argc)
        return NativeFnCallResult{.Result = (f64)std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now().time_since_epoch()).count(), .IsOk = true };
    };
    DefineNativeFun("clock", clock);
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
                if (m_ValueStack.back().HasType<f64>()) m_ValueStack.back().As<f64>() *= -1;
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
                // it hurts me as much as it does you, but this is the fastest way
                Value b = m_ValueStack.back(); m_ValueStack.pop_back();
                Value a = m_ValueStack.back(); m_ValueStack.pop_back();
                if (a.HasType<f64>())
                {
                    if (b.HasType<f64>()) m_ValueStack.emplace_back(a.As<f64>() + b.As<f64>());
                    else if (b.HasType<u64>()) m_ValueStack.emplace_back(a.As<f64>() + (f64)b.As<u64>());
                    else { RuntimeError("Expected strings or numbers."); return InterpretResult::RuntimeError; }
                }
                else if (a.HasType<u64>())
                {
                    if (b.HasType<f64>()) m_ValueStack.emplace_back((f64)a.As<u64>() + b.As<f64>());
                    else if (b.HasType<u64>()) m_ValueStack.emplace_back((f64)a.As<u64>() + (f64)b.As<u64>());
                    else { RuntimeError("Expected strings or numbers."); return InterpretResult::RuntimeError; }
                }
                else if (a.HasType<ObjHandle>())
                {
                    if (b.HasType<ObjHandle>() && a.As<ObjHandle>().HasType<StringObj>() && b.As<ObjHandle>().HasType<StringObj>()) m_ValueStack.emplace_back(AddString(a.As<ObjHandle>().As<StringObj>().String + b.As<ObjHandle>().As<StringObj>().String));
                    else { RuntimeError("Expected strings or numbers."); return InterpretResult::RuntimeError; }
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
                u32 count = (u32)m_ValueStack.back().As<u64>(); m_ValueStack.pop_back();
                m_ValueStack.erase(m_ValueStack.end() - count, m_ValueStack.end());
                break;
            }
        case OpCode::OpDefineGlobal:
            {
                ObjHandle varName = ReadConstant().As<ObjHandle>();
                m_GlobalsSparseSet.Set(varName, m_ValueStack.back()); m_ValueStack.pop_back();
                break;
            }
        case OpCode::OpDefineGlobal32:
            {
                ObjHandle varName = ReadLongConstant().As<ObjHandle>();
                m_GlobalsSparseSet.Set(varName, m_ValueStack.back()); m_ValueStack.pop_back();
                break;
            }  
        case OpCode::OpReadGlobal:
            {
                ObjHandle varName = ReadConstant().As<ObjHandle>();
                if (!m_GlobalsSparseSet.Has(varName))
                {
                    RuntimeError(std::format("Variable \"{}\" is not defined", varName.As<StringObj>().String));
                    return InterpretResult::RuntimeError;
                }
                m_ValueStack.push_back(m_GlobalsSparseSet[varName]);
                break;
            }
        case OpCode::OpReadGlobal32:
            {
                ObjHandle varName = ReadLongConstant().As<ObjHandle>();
                if (!m_GlobalsSparseSet.Has(varName))
                {
                    RuntimeError(std::format("Variable \"{}\" is not defined", varName.As<StringObj>().String));
                    return InterpretResult::RuntimeError;
                }
                m_ValueStack.push_back(m_GlobalsSparseSet[varName]);
                break;
            }
        case OpCode::OpSetGlobal:
            {
                ObjHandle varName = ReadConstant().As<ObjHandle>();
                if (!m_GlobalsSparseSet.Has(varName))
                {
                    RuntimeError(std::format("Variable \"{}\" is not defined", varName.As<StringObj>().String));
                    return InterpretResult::RuntimeError;
                }
                m_GlobalsSparseSet[varName] = m_ValueStack.back();
                break;
            }
        case OpCode::OpSetGlobal32:
            {
                ObjHandle varName = ReadLongConstant().As<ObjHandle>();
                if (!m_GlobalsSparseSet.Has(varName))
                {
                    RuntimeError(std::format("Variable \"{}\" is not defined", varName.As<StringObj>().String));
                    return InterpretResult::RuntimeError;
                }
                m_GlobalsSparseSet[varName] = m_ValueStack.back();
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
        case OpCode::OpReadUpvalue:
            {
                u32 upvalueIndex = ReadByte();
                UpvalueObj& upval = frame->Closure->Upvalues[upvalueIndex].As<UpvalueObj>();
                u32 isClosed = upval.Location == &upval.Closed;
                Value* loc = isClosed ? upval.Location : &m_ValueStack[upval.Index];
                m_ValueStack.emplace_back(*loc);
                break;
            }
        case OpCode::OpSetUpvalue:
            {
                u32 upvalueIndex = ReadByte();
                UpvalueObj& upval = frame->Closure->Upvalues[upvalueIndex].As<UpvalueObj>();
                u32 isClosed = upval.Location == &upval.Closed;
                Value* loc = isClosed ? upval.Location : &m_ValueStack[upval.Index];
                *loc = m_ValueStack.back();
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
        case OpCode::OpClosure:
            {
                ObjHandle fun = m_ValueStack.back().As<ObjHandle>();
                ObjHandle closure = ObjRegistry::Create<ClosureObj>(fun);
                m_ValueStack.pop_back();
                m_ValueStack.emplace_back(closure);
                for (u32 i = 0; i < fun.As<FunObj>().UpvalueCount; i++)
                {
                    bool isLocal = (bool)ReadByte();
                    u8 upvalueIndex = ReadByte();
                    if (isLocal) closure.As<ClosureObj>().Upvalues[i] = CaptureUpvalue(frame->Slot + upvalueIndex);
                    else closure.As<ClosureObj>().Upvalues[i] = frame->Closure->Upvalues[upvalueIndex];
                }
                break;
            }
        case OpCode::OpCloseUpvalue:
            CloseUpvalues((u32)m_ValueStack.size() - 1);
            break;
        case OpCode::OpReturn:
            {
                Value funRes = m_ValueStack.back(); m_ValueStack.pop_back();
                u32 frameSlot = frame->Slot;
                CloseUpvalues(frameSlot);
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
    if (callee.HasType<ObjHandle>())
    {
        ObjHandle obj = callee.As<ObjHandle>();
        if (obj.HasType<FunObj>()) return Call(obj.As<FunObj>(), argc);
        if (obj.HasType<ClosureObj>()) return ClosureCall(obj.As<ClosureObj>(), argc);
        if (obj.HasType<NativeFunObj>()) return NativeCall(obj.As<NativeFunObj>(), argc);
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
    callFrame.Slot = (u32)m_ValueStack.size() - 1 - argc;
    m_CallFrames.push_back(callFrame);
    return true;
}

bool VirtualMachine::ClosureCall(ClosureObj& closure, u8 argc)
{
    bool success = Call(closure.Fun.As<FunObj>(), argc);
    if (success) m_CallFrames.back().Closure = &closure;
    return success;
}

bool VirtualMachine::NativeCall(NativeFunObj& fun, u8 argc)
{
    NativeFnCallResult res = fun.NativeFn(argc, &m_ValueStack[m_ValueStack.size() - 1 - argc]);
    if (!res.IsOk) return false;
    m_ValueStack.erase(m_ValueStack.end() - 1 - argc, m_ValueStack.end());
    m_ValueStack.push_back(res.Result);
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

ObjHandle VirtualMachine::CaptureUpvalue(u32 index)
{
    ObjHandle curr;
    ObjHandle prev = ObjHandle::NonHandle();
    for (curr = m_OpenUpvalues; curr != ObjHandle::NonHandle(); curr = curr.As<UpvalueObj>().Next)
    {
        if (curr.As<UpvalueObj>().Index <= index) break;
        prev = curr;
    }
    if (curr != ObjHandle::NonHandle() && curr.As<UpvalueObj>().Index == index)
    {
        // we already have an upvalue for that local variable
        return curr;
    }
    ObjHandle upvalue = ObjRegistry::Create<UpvalueObj>();
    upvalue.As<UpvalueObj>().Index = index;
    upvalue.As<UpvalueObj>().Next = curr;
    if (prev == ObjHandle::NonHandle()) m_OpenUpvalues = upvalue;
    else prev.As<UpvalueObj>().Next = upvalue;
    return upvalue;
}

void VirtualMachine::CloseUpvalues(u32 last)
{
    for (; m_OpenUpvalues != ObjHandle::NonHandle(); m_OpenUpvalues = m_OpenUpvalues.As<UpvalueObj>().Next)
    {
        if (m_OpenUpvalues.As<UpvalueObj>().Index < last) break;
        m_OpenUpvalues.As<UpvalueObj>().Closed = m_ValueStack[m_OpenUpvalues.As<UpvalueObj>().Index];
        m_OpenUpvalues.As<UpvalueObj>().Location = &m_OpenUpvalues.As<UpvalueObj>().Closed;
    }
}

ObjHandle VirtualMachine::AddString(const std::string& val)
{
    if (m_InternedStrings.contains(val)) return m_InternedStrings.at(val);
    ObjHandle newString = ObjRegistry::Create<StringObj>(val);
    m_InternedStrings.emplace(val, newString);
    return newString;
}

void VirtualMachine::DefineNativeFun(const std::string& name, NativeFn nativeFn)
{
    m_ValueStack.push_back(AddString(std::string{name}));
    ObjHandle funName = m_ValueStack.back().As<ObjHandle>();
    m_ValueStack.push_back(ObjRegistry::Create<NativeFunObj>(nativeFn));
    ObjHandle fun = m_ValueStack.back().As<ObjHandle>();
    m_GlobalsSparseSet.Set(funName, fun);
    m_ValueStack.pop_back();
    m_ValueStack.pop_back();
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
    if (val.HasType<bool>()) return !val.As<bool>();
    if (val.HasType<void*>()) return true;
    return false;
}

bool VirtualMachine::AreEqual(Value a, Value b) const
{
    using objCompFn = bool (*)(ObjHandle, ObjHandle);
    objCompFn objComparisons[(u32)ObjType::Count][(u32)ObjType::Count] = {{nullptr}};
    objComparisons[(u32)ObjType::String][(u32)ObjType::String] = [](ObjHandle strA, ObjHandle strB)
    {
        return strA == strB;
    };
    // it hurts me as much as it does you, but this is the fastest way
    if (a.HasType<bool>())
    {
        if (b.HasType<bool>()) return a.As<bool>() == b.As<bool>();
        return false;
    }
    if (a.HasType<f64>())
    {
        if (b.HasType<f64>()) return a.As<f64>() == b.As<f64>();
        if (b.HasType<u64>()) return a.As<f64>() == (f64)b.As<u64>();
        return false;
    }
    if (a.HasType<u64>())
    {
        if (b.HasType<f64>()) return (f64)a.As<u64>() == b.As<f64>();
        if (b.HasType<u64>()) return a.As<u64>() == b.As<u64>();
        return false;
    }
    if (a.HasType<void*>())
    {
        return b.HasType<void*>();
    }
    if (a.HasType<ObjHandle>())
    {
        if (b.HasType<ObjHandle>()) return objComparisons[(u32)a.As<ObjHandle>().GetType()][(u32)b.As<ObjHandle>().GetType()](a.As<ObjHandle>(), b.As<ObjHandle>());
    }
    return false;

}

#undef BINARY_OP
