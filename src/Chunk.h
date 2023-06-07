#pragma once

#include <string>
#include <vector>

#include "Types.h"
#include "Value.h"

enum class OpCode : u8
{
    OpConstant,
    OpConstantLong,
    OpReturn,
};

struct RunLengthLines
{
    u32 Count;
    u32 Line;
    RunLengthLines(u32 count, u32 line) : Count(count), Line((line)) {}
};

class Chunk
{
    friend class Disassembler;
public:
    Chunk(const std::string& name);
    void AddOperation(OpCode opcode, u32 line);
    // adds constant AND instruction
    void AddConstant(Value val, u32 line);
private:
    u32 GetLine(u32 instructionIndex) const;
    void PushLine(u32 line, u32 count = 1);
    u32 PushConstant(Value val);
private:
    std::string m_Name;
    std::vector<u8> m_Bytes;
    std::vector<f64> m_Values;
    std::vector<RunLengthLines> m_Lines;

    static constexpr u32 MAX_VALUES_COUNT = 1u << 24;
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
public:
    static void Disassemble(const Chunk& chunk);
    static u32 DisassembleInstruction(const Chunk& chunk, u32 offset);
private:
    static u32 SimpleInstruction(const Chunk& chunk, const InstructionInfo& info);
    static u32 ConstantInstruction(const Chunk& chunk, const InstructionInfo& info);
};