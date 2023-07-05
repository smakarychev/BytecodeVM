#pragma once

#include <string>
#include <vector>

#include "OpCode.h"
#include "Value.h"

struct RunLengthLines
{
    u32 Count;
    u32 Line;
    RunLengthLines(u32 count, u32 line) : Count(count), Line((line)) {}
};

class Chunk
{
    friend class Disassembler;
    friend class VirtualMachine;
    friend class Compiler;
public:
    Chunk(const std::string& name = "Default");
    void AddByte(u8 byte, u32 line);
    void AddInt(i32 val, u32 line);
    void AddOperation(OpCode opcode, u32 line);
    // adds operation and it's operand as an index to Value array
    void AddOperation(OpCode opcode, u32 index, u32 line);
    // adds `val` to Values array
    u32 AddConstant(Value val);
    const std::vector<Value>& GetValues() const;
    std::vector<Value>& GetValues();
    u32 CodeLength() const { return (u32)m_Code.size(); }
    std::string_view GetName() const { return m_Name; }
private:
    u32 GetLine(u32 instructionIndex) const;
    void PushLine(u32 line, u32 count = 1);
    // adds val to the Values array AND adds it's index and appropriate operation to code
    void AddOperand(OpCode opCodeShort, u32 index, u32 line);
    u32 PushConstant(Value val);
    OpCode GetLongVariant(OpCode opCode) const;
private:
    std::string m_Name;
    std::vector<u8> m_Code;
    std::vector<Value> m_Values;
    std::vector<RunLengthLines> m_Lines;

    static constexpr u32 MAX_VALUES_COUNT = std::numeric_limits<u32>::max();
    static constexpr u8  BYTE_SHIFT = 8;
};

class Disassembler
{
public:
    struct InstructionInfo
    {
        std::string_view OpName;
        u8 Instruction;
        u32 Offset;
    };
    struct State
    {
        OpCode LastOpCode;
    };
public:
    static void Disassemble(const Chunk& chunk);
    static u32 DisassembleInstruction(const Chunk& chunk, u32 offset);
private:
    static u32 SimpleInstruction(const Chunk& chunk, const InstructionInfo& info);
    static u32 ConstantInstruction(const Chunk& chunk, const InstructionInfo& info);
    static u32 NameInstruction(const Chunk& chunk, const InstructionInfo& info);
    static u32 ByteInstruction(const Chunk& chunk, const InstructionInfo& info);
    static u32 IntInstruction(const Chunk& chunk, const InstructionInfo& info);
    static u32 JumpInstruction(const Chunk& chunk, const InstructionInfo& info);
    static u32 ClosureInstruction(const Chunk& chunk, const InstructionInfo& info);
private:
    static State s_State;
};