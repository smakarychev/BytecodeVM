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
    m_Bytes.push_back(static_cast<u8>(opcode));
    m_Lines.push_back(line);
}

void Chunk::AddConstant(Value val, u32 line)
{
    m_Lines.push_back(line);
    // we can address up to 256 (1 << 8) values, by storing index as one byte,
    // if we have more than 256 values, we need to store index as 24-bit value,
    // and use different opcode for long value operation
    u32 index = PushConstant(val);
    BCVM_ASSERT(index < MAX_VALUES_COUNT, "Cannot store more than {} values.", MAX_VALUES_COUNT)
    if (index < (1u << BYTE_SHIFT))
    {
        m_Bytes.push_back(static_cast<u8>(OpCode::OpConstant));
        m_Bytes.push_back(static_cast<u8>(index));
    }
    else
    {
        u8 mask = ~0;
        u8 byteA = ((index >> 0 * BYTE_SHIFT) & mask);
        u8 byteB = ((index >> 1 * BYTE_SHIFT) & mask);
        u8 byteC = ((index >> 2 * BYTE_SHIFT) & mask);
        m_Bytes.push_back(static_cast<u8>(OpCode::OpConstantLong));
        m_Bytes.push_back(byteA);
        m_Bytes.push_back(byteB);
        m_Bytes.push_back(byteC);
    }
}

u32 Chunk::PushConstant(Value val)
{
    u32 index = (u32)(m_Values.size());
    m_Values.push_back(val);
    return index;
}

void Disassembler::Disassemble(const Chunk& chunk)
{
    m_Chunk = &chunk;
    std::cout << std::format("{:=^33}\n", chunk.m_Name);
    std::cout << std::format("{:>5} {:>6} {:<20}\n", "Offset", "Line", "Opcode");
    std::cout << std::format("{:->33}\n", "");
    u32 lineOffset = 0;
    for (u32 offset = 0; offset < chunk.m_Bytes.size();)
    {
        std::cout << std::format("{:0>5}: ", offset);
        if (lineOffset == 0 || chunk.m_Lines[lineOffset] != chunk.m_Lines[lineOffset - 1])
        {
            std::cout << std::format("{:>5}: ", chunk.m_Lines[lineOffset]);
        }
        else
        {
            std::cout << std::format("{:>5}: ", "|");
        }
        offset = DisassembleInstruction(offset);
        lineOffset++;
    }
}

u32 Disassembler::DisassembleInstruction(u32 offset) const
{
    u8 instruction = m_Chunk->m_Bytes[offset];
    auto opcode = static_cast<OpCode>(instruction);
    switch (opcode)
    {
    case OpCode::OpReturn:
        return SimpleInstruction("OpReturn", instruction, offset);
    case OpCode::OpConstant:
        return ConstantInstruction("OpConstant", instruction, offset);
    case OpCode::OpConstantLong:
        return ConstantInstruction("OpConstantLong", instruction, offset);
    }
    return SimpleInstruction("OpUnknown", instruction, offset);
}

u32 Disassembler::SimpleInstruction(std::string_view opName, u8 instruction, u32 offset) const
{
    std::cout << std::format("[0x{:02x}] {:<15}\n", instruction, opName);
    return offset + 1;
}

u32 Disassembler::ConstantInstruction(std::string_view opName, u8 instruction, u32 offset) const
{
    std::cout << std::format("[0x{:02x}] {:<15} ", instruction, opName);
    if (static_cast<OpCode>(instruction) == OpCode::OpConstant)
    {
        u8 index = m_Chunk->m_Bytes[offset + 1];
        std::cout << std::format("[0x{:02x}] {}\n", index, m_Chunk->m_Values[index]);
        return offset + 2;
    }
    auto& bytes = m_Chunk->m_Bytes;
    u8 shift = Chunk::BYTE_SHIFT;
    u32 index = bytes[offset + 1] | (bytes[offset + 2] << 1 * shift) | (bytes[offset + 3] << 2 * shift);
    std::cout << std::format("[0x{:06x}] {}\n", index, m_Chunk->m_Values[index]);
    return offset + 4;
}
