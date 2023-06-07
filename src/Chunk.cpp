#include "Chunk.h"

#include <format>
#include <iostream>

#include "Core.h"

Chunk::Chunk(const std::string& name)
    : m_Name(name)
{
}

void Chunk::AddOperation(OpCode opcode, u32 line)
{
    m_Code.push_back(static_cast<u8>(opcode));
    PushLine(line);
}

void Chunk::AddConstant(Value val, u32 line)
{
    // we can address up to 256 (1 << 8) values, by storing index as one byte,
    // if we have more than 256 values, we need to store index as 24-bit value,
    // and use different opcode for long value operation
    u32 index = PushConstant(val);
    BCVM_ASSERT(index < MAX_VALUES_COUNT, "Cannot store more than {} values.", MAX_VALUES_COUNT)
    if (index < (1u << BYTE_SHIFT))
    {
        m_Code.push_back(static_cast<u8>(OpCode::OpConstant));
        m_Code.push_back(static_cast<u8>(index));
        PushLine(line, 2);
    }
    else
    {
        u8 mask = ~0;
        u8 byteA = ((index >> 0 * BYTE_SHIFT) & mask);
        u8 byteB = ((index >> 1 * BYTE_SHIFT) & mask);
        u8 byteC = ((index >> 2 * BYTE_SHIFT) & mask);
        m_Code.push_back(static_cast<u8>(OpCode::OpConstantLong));
        m_Code.push_back(byteA);
        m_Code.push_back(byteB);
        m_Code.push_back(byteC);
        PushLine(line, 4);
    }
}

u32 Chunk::GetLine(u32 instructionIndex) const
{
    BCVM_ASSERT(instructionIndex < m_Code.size(), "Invalid instruction index {}.", instructionIndex)
    u32 lineNum = 0;
    u32 processedTotal = 0;
    instructionIndex += 1;
    for(;;)
    {
        RunLengthLines line = m_Lines[lineNum];
        processedTotal += line.Count;
        if (processedTotal >= instructionIndex) return line.Line;
        lineNum++;
    }
}

void Chunk::PushLine(u32 line, u32 count)
{
    if (m_Lines.empty() || line != m_Lines.back().Line) m_Lines.emplace_back(count, line);
    else m_Lines.back().Count += count;
}

u32 Chunk::PushConstant(Value val)
{
    u32 index = (u32)(m_Values.size());
    m_Values.push_back(val);
    return index;
}

void Disassembler::Disassemble(const Chunk& chunk)
{
    std::cout << std::format("{:=^33}\n", chunk.m_Name);
    std::cout << std::format("{:>5} {:>6} {:<20}\n", "Offset", "Line", "Opcode");
    std::cout << std::format("{:->33}\n", "");
    u32 prevLine = std::numeric_limits<u32>::max();
    for (u32 offset = 0; offset < chunk.m_Code.size();)
    {
        std::cout << std::format("{:0>5}: ", offset);
        u32 line = chunk.GetLine(offset);
        if (line != prevLine) std::cout << std::format("{:>5}: ", line);
        else std::cout << std::format("{:>5}: ", "|");
        std::cout << std::format("{:>5}: ", line);
        offset = DisassembleInstruction(chunk, offset);
        prevLine = line;
    }
}

u32 Disassembler::DisassembleInstruction(const Chunk& chunk, u32 offset)
{
    u8 instruction = chunk.m_Code[offset];
    switch (static_cast<OpCode>(instruction))
    {
    case OpCode::OpReturn:
        return SimpleInstruction(chunk, InstructionInfo{"OpReturn", instruction, offset});
    case OpCode::OpConstant:
        return ConstantInstruction(chunk, InstructionInfo{"OpConstant", instruction, offset});
    case OpCode::OpConstantLong:
        return ConstantInstruction(chunk, InstructionInfo{"OpConstantLong", instruction, offset});
    case OpCode::OpNegate:
        return SimpleInstruction(chunk, InstructionInfo{"OpNegate", instruction, offset});
    case OpCode::OpAdd: 
        return SimpleInstruction(chunk, InstructionInfo{"OpAdd", instruction, offset});
    case OpCode::OpSubtract:
        return SimpleInstruction(chunk, InstructionInfo{"OpSubtract", instruction, offset});
    case OpCode::OpMultiply: 
        return SimpleInstruction(chunk, InstructionInfo{"OpMultiply", instruction, offset});
    case OpCode::OpDivide: 
        return SimpleInstruction(chunk, InstructionInfo{"OpDivide", instruction, offset});
    }
    return SimpleInstruction(chunk, InstructionInfo{"OpUnknown", instruction, offset});
}

u32 Disassembler::SimpleInstruction(const Chunk& chunk, const InstructionInfo& info)
{
    std::cout << std::format("[0x{:02x}] {:<15}\n", info.Instruction, info.OpName);
    return info.Offset + 1;
}

u32 Disassembler::ConstantInstruction(const Chunk& chunk, const InstructionInfo& info)
{
    std::cout << std::format("[0x{:02x}] {:<15} ", info.Instruction, info.OpName);
    if (static_cast<OpCode>(info.Instruction) == OpCode::OpConstant)
    {
        u8 index = chunk.m_Code[info.Offset + 1];
        std::cout << std::format("[0x{:02x}] {}\n", index, chunk.m_Values[index]);
        return info.Offset + 2;
    }
    auto& bytes = chunk.m_Code;
    u8 shift = Chunk::BYTE_SHIFT;
    u32 index = bytes[info.Offset + 1] | (bytes[info.Offset + 2] << 1 * shift) | (bytes[info.Offset + 3] << 2 * shift);
    std::cout << std::format("[0x{:06x}] {}\n", index, chunk.m_Values[index]);
    return info.Offset + 4;
}
