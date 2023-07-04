#pragma once

#include "Chunk.h"
#include "Obj.h"
#include "Value.h"
#include "Common/ObjSparseSet.h"

#include <unordered_map>

class Chunk;

enum class InterpretResult { Ok, CompileError, RuntimeError };

struct CallFrame
{
    FunObj* Fun{nullptr};
    ClosureObj* Closure{nullptr};
    u8* Ip{nullptr};
    u32 Slot{0};
};

class VirtualMachine
{
    friend class Compiler;
public:
    VirtualMachine();
    ~VirtualMachine();
    void Init();
    void Repl();
    void RunFile(std::string_view path);
    InterpretResult Interpret(std::string_view source);
private:
    void InitNativeFunctions();
    InterpretResult Run();
    bool CallValue(Value callee, u8 argc);
    bool Call(FunObj& fun, u8 argc);
    bool ClosureCall(ClosureObj& closure, u8 argc);
    bool NativeCall(NativeFunObj& fun, u8 argc);
    
    OpCode ReadInstruction();
    Value ReadConstant();
    Value ReadLongConstant();
    u8 ReadByte();
    i32 ReadI32();
    u32 ReadU32();
    void PrintValue(Value val);
    
    ObjHandle CaptureUpvalue(u32 index);
    void CloseUpvalues(u32 last);
    
    ObjHandle AddString(const std::string& val);
    void DefineNativeFun(const std::string& name, NativeFn nativeFn);
    
    void ClearStack();

    void RuntimeError(const std::string& message);
    
    bool IsFalsey(Value val) const;
    bool AreEqual(Value a, Value b) const;
private:
    std::vector<CallFrame> m_CallFrames;
    std::vector<Value> m_ValueStack;
    std::unordered_map<std::string, ObjHandle> m_InternedStrings;
    ObjSparseSet m_GlobalsSparseSet;

    ObjHandle m_OpenUpvalues{};
};
