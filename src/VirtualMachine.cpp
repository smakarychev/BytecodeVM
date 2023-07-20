#include "VirtualMachine.h"

#include <format>
#include <iostream>
#include <fstream>
#include <ranges>

#include "Compiler.h"
#include "Core.h"
#include "NativeFunctions.h"
#include "Scanner.h"
#include "ValueFormatter.h"

#define BINARY_OP(stack, op)  \
    { \
        Value b = (stack).back(); (stack).pop_back(); \
        Value a = (stack).back(); (stack).pop_back(); \
        if (a.HasType<f64>() && b.HasType<f64>()) \
        { \
            (stack).emplace_back(a.As<f64>() op b.As<f64>()); \
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
    ClearStacks();
    GCContext gcContext = {};
    gcContext.VM = this;
    GarbageCollector::InitContext(gcContext);
    InitNativeFunctions();
    m_InitString = AddString("init");
}

void VirtualMachine::Repl()
{
    for (;;)
    {
        std::cout << "\n> ";
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
    if (result != InterpretResult::Ok) ClearStacks(); 
    if (result == InterpretResult::CompileError) exit(65);
    if (result == InterpretResult::RuntimeError) exit(70);
}

InterpretResult VirtualMachine::Interpret(std::string_view source)
{
    Compiler compiler(this);
    GarbageCollector::GetContext().Compiler = &compiler;
    compiler.Init();
    
    CompilerResult compilerResult = compiler.Compile(source);
    if (!compilerResult.IsOk()) return InterpretResult::CompileError;
    
    m_ValueStack.emplace_back(compilerResult.Get());
    m_CallFrames.push_back({.Fun = compilerResult.Get(), .Ip = compilerResult.Get().As<FunObj>().Chunk.m_Code.data(), .Slot = 0});
    
    return Run();
}

void VirtualMachine::InitNativeFunctions()
{
    DefineNativeFun("print", NativeFunctions::Print);
    DefineNativeFun("println", NativeFunctions::PrintLn);
    DefineNativeFun("input", NativeFunctions::Input);
    DefineNativeFun("clock", NativeFunctions::Clock);
    DefineNativeFun("sleep", NativeFunctions::Sleep);
    DefineNativeFun("str", NativeFunctions::Str);
    DefineNativeFun("int", NativeFunctions::Int);
    DefineNativeFun("float", NativeFunctions::Float);
    DefineNativeFun("rand", NativeFunctions::Rand);
    DefineNativeFun("len", NativeFunctions::Len);
}

InterpretResult VirtualMachine::Run()
{
    CallFrame* frame = &m_CallFrames.back();
    for(;;)
    {
#ifdef DEBUG_TRACE
        std::cout << "\nStack trace: ";
        for (auto& v : m_ValueStack) { std::cout << std::format("[{}] ", v); }
        std::cout << "\n";
        Disassembler::DisassembleInstruction(frame->Fun.As<FunObj>().Chunk, (u32)(frame->Ip - frame->Fun.As<FunObj>().Chunk.m_Code.data()));
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
                if (m_ValueStack.back().HasType<f64>())
                {
                    f64 val = m_ValueStack.back().As<f64>(); m_ValueStack.pop_back();
                    m_ValueStack.push_back(-val);
                }
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
                if (a.HasType<f64>() && b.HasType<f64>())
                {
                    m_ValueStack.emplace_back(a.As<f64>() + b.As<f64>());
                }
                else if (a.HasType<ObjHandle>() && b.HasType<ObjHandle>() &&
                    a.As<ObjHandle>().HasType<StringObj>() && b.As<ObjHandle>().HasType<StringObj>())
                {
                    m_ValueStack.emplace_back(AddString(a.As<ObjHandle>().As<StringObj>().String + b.As<ObjHandle>().As<StringObj>().String));
                }
                else
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
        case OpCode::OpPop:
            m_ValueStack.pop_back();
            break;
        case OpCode::OpPopN:
            {
                u32 count = (u32)m_ValueStack.back().As<f64>(); m_ValueStack.pop_back();
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
                UpvalueObj& upval = frame->Closure.As<ClosureObj>().Upvalues[upvalueIndex].As<UpvalueObj>();
                u32 isClosed = upval.Location == &upval.Closed;
                Value* loc = isClosed ? upval.Location : &m_ValueStack[upval.Index];
                m_ValueStack.emplace_back(*loc);
                break;
            }
        case OpCode::OpSetUpvalue:
            {
                u32 upvalueIndex = ReadByte();
                UpvalueObj& upval = frame->Closure.As<ClosureObj>().Upvalues[upvalueIndex].As<UpvalueObj>();
                u32 isClosed = upval.Location == &upval.Closed;
                Value* loc = isClosed ? upval.Location : &m_ValueStack[upval.Index];
                *loc = m_ValueStack.back();
                break;
            }
        case OpCode::OpReadProperty:
            {
                Value iVal = m_ValueStack.back(); 
                if (!(iVal.HasType<ObjHandle>() && iVal.As<ObjHandle>().HasType<InstanceObj>()))
                {
                    RuntimeError("Only instances have properties.");
                    return InterpretResult::RuntimeError;
                }
                auto instance = iVal.As<ObjHandle>();
                ObjHandle prop = ReadConstant().As<ObjHandle>();
                if (ReadField(instance, prop)) break;
                if (ReadMethod(instance.As<InstanceObj>().Class, prop)) break;
                RuntimeError(std::format("Unknown property: {}.", prop));
                return InterpretResult::RuntimeError;
            }
        case OpCode::OpReadProperty32:
            {
                Value iVal = m_ValueStack.back(); m_ValueStack.pop_back();
                if (!(iVal.HasType<ObjHandle>() && iVal.As<ObjHandle>().HasType<InstanceObj>()))
                {
                    RuntimeError("Only instances have properties.");
                    return InterpretResult::RuntimeError;
                }
                auto instance = iVal.As<ObjHandle>();
                ObjHandle prop = ReadLongConstant().As<ObjHandle>();
                if (ReadField(instance, prop)) break;
                if (ReadMethod(instance.As<InstanceObj>().Class, prop)) break;
                RuntimeError(std::format("Unknown property: {}.", prop));
                return InterpretResult::RuntimeError;
            }
        case OpCode::OpSetProperty:
            {
                Value iVal = m_ValueStack[m_ValueStack.size() - 2];
                if (!(iVal.HasType<ObjHandle>() && iVal.As<ObjHandle>().HasType<InstanceObj>()))
                {
                    RuntimeError("Only instances have properties.");
                    return InterpretResult::RuntimeError;
                }
                auto& instance = iVal.As<ObjHandle>().As<InstanceObj>();
                ObjHandle prop = ReadConstant().As<ObjHandle>();
                instance.Fields.Set(prop, m_ValueStack.back()); m_ValueStack.pop_back();
                m_ValueStack.pop_back(); // pop instance
                m_ValueStack.push_back(instance.Fields[prop]); // push val back to stack for subsequent sets.
                break;
            }
        case OpCode::OpSetProperty32:
            {
                Value iVal = m_ValueStack[m_ValueStack.size() - 2];
                if (!(iVal.HasType<ObjHandle>() && iVal.As<ObjHandle>().HasType<InstanceObj>()))
                {
                    RuntimeError("Only instances have properties.");
                    return InterpretResult::RuntimeError;
                }
                auto& instance = iVal.As<ObjHandle>().As<InstanceObj>();
                ObjHandle prop = ReadLongConstant().As<ObjHandle>();
                instance.Fields.Set(prop, m_ValueStack.back()); m_ValueStack.pop_back();
                m_ValueStack.pop_back(); // pop instance
                m_ValueStack.push_back(instance.Fields[prop]); // push val back to stack for subsequent sets.
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
                    RuntimeError("Error during call.");
                    return InterpretResult::RuntimeError;
                }
                frame = &m_CallFrames.back();
                break;
            }
        case OpCode::OpInvoke:
            {
                ObjHandle method = m_ValueStack.back().As<ObjHandle>(); m_ValueStack.pop_back();
                u8 argc = ReadByte();
                if (!Invoke(method, argc))
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
                    else closure.As<ClosureObj>().Upvalues[i] = frame->Closure.As<ClosureObj>().Upvalues[upvalueIndex];
                }
                break;
            }
        case OpCode::OpCloseUpvalue:
            CloseUpvalues((u32)m_ValueStack.size() - 1);
            break;
        case OpCode::OpClass:
            {
                ObjHandle classObj = ObjRegistry::Create<ClassObj>(m_ValueStack.back().As<ObjHandle>());
                m_ValueStack.pop_back();
                m_ValueStack.emplace_back(classObj);
                break;
            }
        case OpCode::OpInherit:
            {
                usize stackTop = m_ValueStack.size() - 1;
                ObjHandle superClassHandle = m_ValueStack[stackTop - 1].As<ObjHandle>();
                if (!superClassHandle.HasType<ClassObj>())
                {
                    RuntimeError("Superclass must be a class.");
                    return InterpretResult::RuntimeError;
                }
                ClassObj& superClass = superClassHandle.As<ClassObj>();
                ClassObj& subClass = m_ValueStack[stackTop].As<ObjHandle>().As<ClassObj>();
                for (usize i = 0; i < superClass.Methods.m_Sparse.size(); i++)
                {
                    u64 index = superClass.Methods.m_Sparse[i];
                    if (index != ObjSparseSet::SPARSE_NONE)
                    {
                        subClass.Methods.Set(superClass.Methods.GetKey(i), superClass.Methods.GetValue(i));
                    }
                }
                m_ValueStack.pop_back();
                break;
            }
        case OpCode::OpMethod:
            {
                u64 stackTop = m_ValueStack.size() - 1;
                ObjHandle name = m_ValueStack[stackTop].As<ObjHandle>();
                ObjHandle body = m_ValueStack[stackTop - 1].As<ObjHandle>();
                ObjHandle classObj = m_ValueStack[stackTop - 2].As<ObjHandle>();
                classObj.As<ClassObj>().Methods.Set(name, body);
                m_ValueStack.pop_back();
                m_ValueStack.pop_back();
                break;
            }
        case OpCode::OpReadSuper:
            {
                u64 stackTop = m_ValueStack.size() - 1;
                ObjHandle method = m_ValueStack[stackTop].As<ObjHandle>();
                ObjHandle superClass = m_ValueStack[stackTop - 1].As<ObjHandle>();
                m_ValueStack.pop_back(); m_ValueStack.pop_back(); // pop method name and superclass
                if (ReadMethod(superClass, method)) break;
                RuntimeError(std::format("Unknown property: {}.", method));
                return InterpretResult::RuntimeError;
            }
        case OpCode::OpInvokeSuper:
            {
                ObjHandle method = m_ValueStack.back().As<ObjHandle>(); m_ValueStack.pop_back();
                ObjHandle superClass = m_ValueStack.back().As<ObjHandle>(); m_ValueStack.pop_back();
                u8 argc = ReadByte();
                if (!InvokeFromClass(superClass, method, argc))
                {
                    return InterpretResult::RuntimeError;
                }
                frame = &m_CallFrames.back();
                break;
            }
        case OpCode::OpCollection:
            {
                u32 count = (u32)m_ValueStack.back().As<f64>(); m_ValueStack.pop_back();
                ObjHandle collectionH = ObjRegistry::Create<CollectionObj>(count);
                CollectionObj& collection = collectionH.As<CollectionObj>();
                for (i32 i = count - 1; i >= 0; i--)
                {
                    collection.Items[i] = m_ValueStack.back(); m_ValueStack.pop_back();
                }
                m_ValueStack.emplace_back(collectionH);
                break;
            }
        case OpCode::OpReadSubscript:
            {
                Value index = m_ValueStack.back(); m_ValueStack.pop_back();
                Value collection = m_ValueStack.back(); m_ValueStack.pop_back();
                if (!CheckCollectionIndex(collection, index))
                {
                    return InterpretResult::RuntimeError;
                }
                Value sub = GetCollectionSubscript(collection.As<ObjHandle>(), (u32)index.As<f64>());
                if (m_HadError)
                {
                    m_HadError = false;
                    return InterpretResult::RuntimeError;
                }
                m_ValueStack.push_back(sub);
                break;
            }
        case OpCode::OpSetSubscript:
            {
                Value newVal = m_ValueStack.back(); m_ValueStack.pop_back();
                Value index = m_ValueStack.back(); m_ValueStack.pop_back();
                Value collection = m_ValueStack.back(); m_ValueStack.pop_back();
                if (!CheckCollectionIndex(collection, index))
                {
                    return InterpretResult::RuntimeError;
                }
                SetCollectionSubscript(collection.As<ObjHandle>(), (u32)index.As<f64>(), newVal);
                if (m_HadError)
                {
                    m_HadError = false;
                    return InterpretResult::RuntimeError;
                }
                m_ValueStack.push_back(newVal);
                break;
            }
        case OpCode::OpColMultiply:
            {
                usize stackTop = m_ValueStack.size() - 1;
                Value b = m_ValueStack[stackTop];
                Value a = m_ValueStack[stackTop - 1];
                if (a.HasType<f64>() && b.HasType<ObjHandle>())
                    std::swap(a, b);
                if (a.HasType<ObjHandle>() && b.HasType<f64>())
                {
                    if (!(b.As<f64>() >= 0 && std::floor(b.As<f64>()) == (u32)b.As<f64>()))
                    {
                        RuntimeError("Expected positive integer number.");
                        return InterpretResult::RuntimeError; 
                    }
                    u32 number = (u32)b.As<f64>();
                    if (a.As<ObjHandle>().HasType<StringObj>())
                    {
                        const std::string& originalString = a.As<ObjHandle>().As<StringObj>().String;
                        std::string newString;
                        newString.reserve(originalString.size() * number);
                        for (u32 i = 0; i < number; i++)
                        {
                            newString.append(originalString);
                        }
                        ObjHandle newStringH = AddString(newString);
                        m_ValueStack.pop_back();
                        m_ValueStack.pop_back();
                        m_ValueStack.emplace_back(newStringH);
                    }
                    else if (a.As<ObjHandle>().HasType<CollectionObj>())
                    {
                        const CollectionObj& originalCol = a.As<ObjHandle>().As<CollectionObj>();
                        ObjHandle newColH = ObjRegistry::Create<CollectionObj>(originalCol.ItemCount * number);
                        m_ValueStack.emplace_back(newColH);
                        CollectionObj& newCol = newColH.As<CollectionObj>();
                        for (u32 repI = 0; repI < number; repI++)
                        {
                            for (u32 i = 0; i < originalCol.ItemCount; i++)
                            {
                                if (originalCol.Items[i].HasType<ObjHandle>())
                                {
                                    newCol.Items[repI * originalCol.ItemCount + i] = ObjRegistry::Clone(originalCol.Items[i].As<ObjHandle>());
                                }
                                else
                                {
                                    newCol.Items[repI * originalCol.ItemCount + i] = originalCol.Items[i];
                                }
                            }
                        }
                        m_ValueStack.pop_back(); // pop newCollection
                        m_ValueStack.pop_back();
                        m_ValueStack.pop_back();
                        m_ValueStack.emplace_back(newColH); // return newCollection back to stack
                    }
                    else
                    {
                        RuntimeError("Expected collection or string.");
                        return InterpretResult::RuntimeError; 
                    }
                }
                else
                {
                    RuntimeError("Expected one operand to be collection and other to be positive integer number.");
                    return InterpretResult::RuntimeError; 
                }
                break;
            }
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

bool VirtualMachine::Invoke(ObjHandle method, u8 argc)
{
    ObjHandle instanceHandle = m_ValueStack[m_ValueStack.size() - 1 - argc].As<ObjHandle>();
    if (!instanceHandle.HasType<InstanceObj>())
    {
        RuntimeError("Only classes have methods.");
        return false;
    }
    const InstanceObj& instance = instanceHandle.As<InstanceObj>();
    if (instance.Fields.Has(method))
    {
        Value field = instance.Fields[method];
        m_ValueStack[m_ValueStack.size() - 1 - argc] = field;
        return CallValue(field, argc);
    }

    if (!InvokeFromClass(instance.Class, method, argc))
    {
        RuntimeError(std::format("Unknown property: {}.", method));
        return false;    
    }
    return true;
}

bool VirtualMachine::InvokeFromClass(ObjHandle classObj, ObjHandle method, u8 argc)
{
    if (classObj.As<ClassObj>().Methods.Has(method))
    {
        return ClosureCall(classObj.As<ClassObj>().Methods[method].As<ObjHandle>(), argc);
    }
    RuntimeError(std::format("Unknown property: {}.", method));
    return false;
}

bool VirtualMachine::CallValue(Value callee, u8 argc)
{
    if (callee.HasType<ObjHandle>())
    {
        ObjHandle obj = callee.As<ObjHandle>();
        switch (obj.GetType())
        {
        case ObjType::Fun:          return Call(obj, argc);
        case ObjType::Closure:      return ClosureCall(obj, argc);
        case ObjType::NativeFun:    return NativeCall(obj, argc);
        case ObjType::Class:        return ClassCall(obj, argc);
        case ObjType::BoundMethod:  return MethodCall(obj, argc);
        default: break;
        }
    }
    RuntimeError("Can only call functions and classes.");
    return false;
}

bool VirtualMachine::Call(ObjHandle fun, u8 argc)
{
    if (fun.As<FunObj>().Arity != argc)
    {
        RuntimeError(std::format("Expected {} arguments, but got {}.", fun.As<FunObj>().Arity, argc));
        return false;
    }
    CallFrame callFrame;
    callFrame.Fun = fun;
    callFrame.Ip = fun.As<FunObj>().Chunk.m_Code.data();
    callFrame.Slot = (u32)m_ValueStack.size() - 1 - argc;
    m_CallFrames.push_back(callFrame);
    return true;
}

bool VirtualMachine::ClosureCall(ObjHandle closure, u8 argc)
{
    bool success = Call(closure.As<ClosureObj>().Fun, argc);
    if (success) m_CallFrames.back().Closure = closure;
    return success;
}

bool VirtualMachine::NativeCall(ObjHandle fun, u8 argc)
{
    NativeFnCallResult res = fun.As<NativeFunObj>().NativeFn(argc, &m_ValueStack.back() + 1 - argc, this);
    if (!res.IsOk) return false;
    m_ValueStack.erase(m_ValueStack.end() - 1 - argc, m_ValueStack.end());
    m_ValueStack.push_back(res.Result);
    return true;
}

bool VirtualMachine::ClassCall(ObjHandle classObj, u8 argc)
{
    m_ValueStack[m_ValueStack.size() - 1 - argc] = ObjRegistry::Create<InstanceObj>(classObj);
    if (classObj.As<ClassObj>().Methods.Has(m_InitString))
    {
        ClosureCall(classObj.As<ClassObj>().Methods.Get(m_InitString).As<ObjHandle>(), argc);
    }
    else
    {
        if (argc != 0)
        {
            RuntimeError(std::format("Expected 0 arguments, but got {}", argc));
            return false;
        }
    }
    return true;
}

bool VirtualMachine::MethodCall(ObjHandle method, u8 argc)
{
    m_ValueStack[m_ValueStack.size() - 1 - argc] = method.As<BoundMethodObj>().Receiver;
    return ClosureCall(method.As<BoundMethodObj>().Method, argc);
}

bool VirtualMachine::ReadField(ObjHandle instance, ObjHandle prop)
{
    if (instance.As<InstanceObj>().Fields.Has(prop))
    {
        m_ValueStack.pop_back();
        m_ValueStack.push_back(instance.As<InstanceObj>().Fields[prop]);
        return true;
    }
    return false;
}

bool VirtualMachine::ReadMethod(ObjHandle classObj, ObjHandle prop)
{
    if (classObj.As<ClassObj>().Methods.Has(prop))
    {
        ObjHandle boundMethod = ObjRegistry::Create<BoundMethodObj>(m_ValueStack.back().As<ObjHandle>(), classObj.As<ClassObj>().Methods[prop].As<ObjHandle>());
        m_ValueStack.pop_back();
        m_ValueStack.push_back(boundMethod);
        return true;
    }
    return false;
}

bool VirtualMachine::CheckCollectionIndex(const Value& collection, const Value& index)
{
    if (!(index.HasType<f64>() && index.As<f64>() >= 0 && std::floor(index.As<f64>()) == (u32)index.As<f64>()))
    {
        RuntimeError("Only numbers can be used as indices.");
        return false;
    }
    if (!(collection.HasType<ObjHandle>() &&
         (collection.As<ObjHandle>().HasType<StringObj>() ||
          collection.As<ObjHandle>().HasType<CollectionObj>())))
    {
        RuntimeError("Only collections and strings are subscriptable.");
        return false;
    }
    return true;
}

Value VirtualMachine::GetCollectionSubscript(ObjHandle collection, u32 index)
{
    if (collection.HasType<CollectionObj>())
    {
        CollectionObj& collectionObj = collection.As<CollectionObj>();
        if (index >= collectionObj.ItemCount)
        {
            RuntimeError("Subscript index out of range.");
            return nullptr;
        }
        return collectionObj.Items[index];
    }
    else
    {
        // else it is string
        const std::string& string = collection.As<StringObj>().String;
        if (string.size() <= index)
        {
            RuntimeError("Subscript index out of range.");
            return nullptr;
        }
        AddString(std::string{string[index]});
        return m_InternedStrings.at(std::string{string[index]});
    }
}

void VirtualMachine::SetCollectionSubscript(ObjHandle collection, u32 index, const Value& val)
{
    if (collection.HasType<CollectionObj>())
    {
        CollectionObj& collectionObj = collection.As<CollectionObj>();
        if (index >= collectionObj.ItemCount)
        {
            RuntimeError("Subscript index out of range.");
            return;
        }
        collectionObj.Items[index] = val;
        return;
    }
    else
    {
        // else it is string
        std::string& string = collection.As<StringObj>().String;
        if (string.size() <= index)
        {
            RuntimeError("Subscript index out of range.");
            return;
        }
        if (!(val.HasType<ObjHandle>() &&
            val.As<ObjHandle>().HasType<StringObj>() &&
            val.As<ObjHandle>().As<StringObj>().String.size() == 1))
        {
            RuntimeError("Can assign char strings only to StringObj subscript");
            return;
        }
        string[index] = val.As<ObjHandle>().As<StringObj>().String[0];
        return;
    }
}

OpCode VirtualMachine::ReadInstruction()
{
    CallFrame& frame = m_CallFrames.back();
    return static_cast<OpCode>(*frame.Ip++);
}

Value VirtualMachine::ReadConstant()
{
    CallFrame& frame = m_CallFrames.back();
    return frame.Fun.As<FunObj>().Chunk.m_Values[ReadByte()];
}

Value VirtualMachine::ReadLongConstant()
{
    CallFrame& frame = m_CallFrames.back();
    return frame.Fun.As<FunObj>().Chunk.m_Values[ReadU32()];
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

void VirtualMachine::ClearStacks()
{
    m_ValueStack.clear();
    m_CallFrames.clear();
}

void VirtualMachine::RuntimeError(const std::string& message)
{
    std::string errorMessage = std::format("{}\n", message);
    for (auto& m_CallFrame : std::ranges::reverse_view(m_CallFrames))
    {
        u32 instruction = (u32)(m_CallFrame.Ip - m_CallFrame.Fun.As<FunObj>().Chunk.m_Code.data()) - 1;
        u32 line = m_CallFrame.Fun.As<FunObj>().Chunk.GetLine(instruction);
        errorMessage += std::format("[line {}] in {}()\n", line, m_CallFrame.Fun.As<FunObj>().GetName());
    }
    LOG_ERROR("Runtime: {}", errorMessage);
    ClearStacks();
    m_HadError = true;
}

bool VirtualMachine::IsFalsey(Value val) const
{
    if (val.HasType<bool>()) return !val.As<bool>();
    if (val.HasType<void*>()) return true;
    return false;
}

bool VirtualMachine::AreEqual(Value a, Value b) const
{
#ifdef NAN_BOXING
    return a == b;
#else
    using objCompFn = bool (*)(ObjHandle, ObjHandle);
    objCompFn objComparisons[(u32)ObjType::Count][(u32)ObjType::Count] = {{nullptr}};
    objComparisons[(u32)ObjType::String][(u32)ObjType::String] = [](ObjHandle strA, ObjHandle strB)
    {
        return strA == strB;
    };
    if (a.HasType<bool>())
    {
        if (b.HasType<bool>()) return a.As<bool>() == b.As<bool>();
        return false;
    }
    if (a.HasType<f64>())
    {
        if (b.HasType<f64>()) return a.As<f64>() == b.As<f64>();
        return false;
    }
    if (a.HasType<void*>())
    {
        return b.HasType<void*>();
    }
    if (a.HasType<ObjHandle>())
    {
        if (b.HasType<ObjHandle>())
        {
            objCompFn fn = objComparisons[(u32)a.As<ObjHandle>().GetType()][(u32)b.As<ObjHandle>().GetType()];
            if (fn == nullptr) return false;
            return fn(a.As<ObjHandle>(), b.As<ObjHandle>());
        }
    }
    return false;
#endif
}

#undef BINARY_OP
