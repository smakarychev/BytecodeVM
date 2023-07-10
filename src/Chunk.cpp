#include "Chunk.h"

#include <format>
#include <iostream>

#include "Core.h"
#include "Obj.h"
#include "ValueFormatter.h"

Chunk::Chunk(const std::string& name)
    : m_Name(name)
{
}

void Chunk::AddByte(u8 byte, u32 line)
{
    m_Code.push_back(byte);
    PushLine(line);
}

void Chunk::AddInt(i32 val, u32 line)
{
    u8* bytes = reinterpret_cast<u8*>(&val);
    m_Code.push_back(bytes[0]);
    m_Code.push_back(bytes[1]);
    m_Code.push_back(bytes[2]);
    m_Code.push_back(bytes[3]);
    PushLine(line, 4);
}

void Chunk::AddOperation(OpCode opcode, u32 line)
{
    m_Code.push_back(static_cast<u8>(opcode));
    PushLine(line);
}

void Chunk::AddOperation(OpCode opcode, u32 index, u32 line)
{
    AddOperand(opcode, index, line);
}

u32 Chunk::AddConstant(Value val)
{
    return PushConstant(val);
}

const std::vector<Value>& Chunk::GetValues() const
{
    return m_Values;
}

std::vector<Value>& Chunk::GetValues()
{
    return m_Values;
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

void Chunk::AddOperand(OpCode opCodeShort, u32 index, u32 line)
{
    OpCode opCodeLong = GetLongVariant(opCodeShort);
    
    // we can address up to 256 (1 << 8) values, by storing index as one byte,
    // if we have more than 256 values, we need to store index as 32-bit value,
    // and use different opcode for long value operation
    BCVM_ASSERT(index < MAX_VALUES_COUNT, "Cannot store more than {} values.", MAX_VALUES_COUNT)
    if (index < (1u << BYTE_SHIFT))
    {
        m_Code.push_back(static_cast<u8>(opCodeShort));
        m_Code.push_back(static_cast<u8>(index));
        PushLine(line, 2);
    }
    else
    {
        u8* bytes = reinterpret_cast<u8*>(&index);
        m_Code.push_back(static_cast<u8>(opCodeLong));
        m_Code.push_back(bytes[0]);
        m_Code.push_back(bytes[1]);
        m_Code.push_back(bytes[2]);
        m_Code.push_back(bytes[3]);
        PushLine(line, 5);
    }
}

u32 Chunk::PushConstant(Value val)
{
    u32 index = (u32)(m_Values.size());
    m_Values.push_back(val);
    return index;
}

OpCode Chunk::GetLongVariant(OpCode opCode) const
{
    switch (opCode) {
    case OpCode::OpConstant:    return OpCode::OpConstant32;
    case OpCode::OpDefineGlobal:return OpCode::OpDefineGlobal32;
    case OpCode::OpReadGlobal:  return OpCode::OpReadGlobal32;
    case OpCode::OpSetGlobal:   return OpCode::OpSetGlobal32;
    case OpCode::OpReadLocal:   return OpCode::OpReadLocal32;
    case OpCode::OpSetLocal:    return OpCode::OpSetLocal32;
    case OpCode::OpReadUpvalue:
    case OpCode::OpSetUpvalue:  return OpCode::OpConstant32;
    default:
        LOG_ERROR("Opcode has no long variant.");
        return OpCode::OpConstant32;
    }
}

Disassembler::State Disassembler::s_State = Disassembler::State{};

void Disassembler::Disassemble(const Chunk& chunk)
{
    std::cout << std::format("{:=^66}\n", chunk.m_Name);
    std::cout << std::format("{:>5} {:>6} {:<20}\n", "Offset", "Line", "Opcode");
    std::cout << std::format("{:->66}\n", "");
    u32 prevLine = std::numeric_limits<u32>::max();
    for (u32 offset = 0; offset < chunk.m_Code.size();)
    {
        std::cout << std::format("{:0>5}: ", offset);
        u32 line = chunk.GetLine(offset);
        if (line != prevLine) std::cout << std::format("{:>5}: ", line);
        else std::cout << std::format("{:>5}: ", "|");
        offset = DisassembleInstruction(chunk, offset);
        prevLine = line;
    }
    std::cout << std::format("{:->66}\n", "");
}

u32 Disassembler::DisassembleInstruction(const Chunk& chunk, u32 offset)
{
    u8 instruction = chunk.m_Code[offset];
    switch (static_cast<OpCode>(instruction))
    {
    case OpCode::OpReturn:        return SimpleInstruction(chunk, InstructionInfo{"OpReturn", instruction, offset});
    case OpCode::OpConstant:      return ConstantInstruction(chunk, InstructionInfo{"OpConstant", instruction, offset});
    case OpCode::OpConstant32:    return ConstantInstruction(chunk, InstructionInfo{"OpConstant32", instruction, offset});
    case OpCode::OpNil:           return SimpleInstruction(chunk, InstructionInfo{"OpNil", instruction, offset});
    case OpCode::OpFalse:         return SimpleInstruction(chunk, InstructionInfo{"OpFalse", instruction, offset});
    case OpCode::OpTrue:          return SimpleInstruction(chunk, InstructionInfo{"OpTrue", instruction, offset});
    case OpCode::OpNegate:        return SimpleInstruction(chunk, InstructionInfo{"OpNegate", instruction, offset});
    case OpCode::OpNot:           return SimpleInstruction(chunk, InstructionInfo{"OpNot", instruction, offset});
    case OpCode::OpAdd:           return SimpleInstruction(chunk, InstructionInfo{"OpAdd", instruction, offset});
    case OpCode::OpSubtract:      return SimpleInstruction(chunk, InstructionInfo{"OpSubtract", instruction, offset});
    case OpCode::OpMultiply:      return SimpleInstruction(chunk, InstructionInfo{"OpMultiply", instruction, offset});
    case OpCode::OpDivide:        return SimpleInstruction(chunk, InstructionInfo{"OpDivide", instruction, offset});
    case OpCode::OpEqual:         return SimpleInstruction(chunk, InstructionInfo{"OpEqual", instruction, offset});
    case OpCode::OpLess:          return SimpleInstruction(chunk, InstructionInfo{"OpLess", instruction, offset});
    case OpCode::OpLequal:        return SimpleInstruction(chunk, InstructionInfo{"OpLequal", instruction, offset});
    case OpCode::OpPrint:         return SimpleInstruction(chunk, InstructionInfo{"OpPrint", instruction, offset});
    case OpCode::OpPop:           return SimpleInstruction(chunk, InstructionInfo{"OpPop", instruction, offset});
    case OpCode::OpPopN:          return SimpleInstruction(chunk, InstructionInfo{"OpPopN", instruction, offset});
    case OpCode::OpDefineGlobal:  return NameInstruction(chunk, InstructionInfo{"OpDefineGlobal", instruction, offset});
    case OpCode::OpDefineGlobal32:return NameInstruction(chunk, InstructionInfo{"OpDefineGlobal32", instruction, offset});
    case OpCode::OpReadGlobal:    return NameInstruction(chunk, InstructionInfo{"OpReadGlobal", instruction, offset});
    case OpCode::OpReadGlobal32:  return NameInstruction(chunk, InstructionInfo{"OpReadGlobal32", instruction, offset});
    case OpCode::OpSetGlobal:     return NameInstruction(chunk, InstructionInfo{"OpSetGlobal", instruction, offset});
    case OpCode::OpSetGlobal32:   return NameInstruction(chunk, InstructionInfo{"OpSetGlobal32", instruction, offset});
    case OpCode::OpReadLocal:     return ByteInstruction(chunk, InstructionInfo{"OpReadLocal", instruction, offset});
    case OpCode::OpReadLocal32:   return IntInstruction(chunk, InstructionInfo{"OpReadLocal32", instruction, offset});
    case OpCode::OpSetLocal:      return ByteInstruction(chunk, InstructionInfo{"OpSetLocal", instruction, offset});
    case OpCode::OpSetLocal32:    return IntInstruction(chunk, InstructionInfo{"OpSetLocal32", instruction, offset});
    case OpCode::OpReadUpvalue:   return ByteInstruction(chunk, InstructionInfo{"OpReadUpvalue", instruction, offset});
    case OpCode::OpSetUpvalue:    return ByteInstruction(chunk, InstructionInfo{"OpSetUpvalue", instruction, offset});
    case OpCode::OpJump:          return JumpInstruction(chunk, InstructionInfo{"OpJump", instruction, offset});
    case OpCode::OpJumpFalse:     return JumpInstruction(chunk, InstructionInfo{"OpJumpFalse", instruction, offset});
    case OpCode::OpJumpTrue:      return JumpInstruction(chunk, InstructionInfo{"OpJumpTrue", instruction, offset});
    case OpCode::OpClosure:       return ClosureInstruction(chunk, InstructionInfo{"OpClosure", instruction, offset});
    case OpCode::OpCloseUpvalue:  return SimpleInstruction(chunk, InstructionInfo{"OpCloseUpvalue", instruction, offset});
    case OpCode::OpCall:          return ByteInstruction(chunk, InstructionInfo{"OpCall", instruction, offset});
    }
    return SimpleInstruction(chunk, InstructionInfo{"OpUnknown", instruction, offset});
}

u32 Disassembler::SimpleInstruction(const Chunk& chunk, const InstructionInfo& info)
{
    std::cout << std::format("[0x{:02x}] {:<20}\n", info.Instruction, info.OpName);
    s_State.LastOpCode = static_cast<OpCode>(info.Instruction);
    return info.Offset + 1;
}

u32 Disassembler::ConstantInstruction(const Chunk& chunk, const InstructionInfo& info)
{
    std::cout << std::format("[0x{:02x}] {:<20} ", info.Instruction, info.OpName);
    if (static_cast<OpCode>(info.Instruction) == OpCode::OpConstant)
    {
        u8 index = chunk.m_Code[info.Offset + 1];
        std::cout << std::format("[0x{:02x}] {}\n", index, chunk.m_Values[index]);
        s_State.LastOpCode = static_cast<OpCode>(info.Instruction);
        return info.Offset + 2;
    }
    auto& bytes = chunk.m_Code;
    u32 index = *reinterpret_cast<const u32*>(&bytes[info.Offset + 1]);
    std::cout << std::format("[0x{:08x}] {}\n", index, chunk.m_Values[index]);
    s_State.LastOpCode = static_cast<OpCode>(info.Instruction);
    return info.Offset + 5;
}

u32 Disassembler::NameInstruction(const Chunk& chunk, const InstructionInfo& info)
{
    std::cout << std::format("[0x{:02x}] {:<20} ", info.Instruction, info.OpName);
    if (static_cast<OpCode>(info.Instruction) == OpCode::OpReadGlobal ||
        static_cast<OpCode>(info.Instruction) == OpCode::OpSetGlobal  ||
        static_cast<OpCode>(info.Instruction) == OpCode::OpDefineGlobal)
    {
        u8 varNum = chunk.m_Code[info.Offset + 1];
        std::cout << std::format("[{}]\n", chunk.m_Values[varNum].As<ObjHandle>().As<StringObj>().String);
        s_State.LastOpCode = static_cast<OpCode>(info.Instruction);
        return info.Offset + 2;
    }
    auto& bytes = chunk.m_Code;
    u32 varNum = *reinterpret_cast<const u32*>(&bytes[info.Offset + 1]);
    std::cout << std::format("[{}]\n", chunk.m_Values[varNum].As<ObjHandle>().As<StringObj>().String);
    s_State.LastOpCode = static_cast<OpCode>(info.Instruction);
    return info.Offset + 5;
}

u32 Disassembler::ByteInstruction(const Chunk& chunk, const InstructionInfo& info)
{
    std::cout << std::format("[0x{:02x}] {:<20} ", info.Instruction, info.OpName);
    u8 varNum = chunk.m_Code[info.Offset + 1];
    std::cout << std::format("[0x{:02x}]\n", varNum);
    s_State.LastOpCode = static_cast<OpCode>(info.Instruction);
    return info.Offset + 2;
}

u32 Disassembler::IntInstruction(const Chunk& chunk, const InstructionInfo& info)
{
    std::cout << std::format("[0x{:02x}] {:<20} ", info.Instruction, info.OpName);
    auto& bytes = chunk.m_Code;
    u32 varNum = *reinterpret_cast<const u32*>(&bytes[info.Offset + 1]);
    std::cout << std::format("[0x{:02x}]\n", varNum);
    s_State.LastOpCode = static_cast<OpCode>(info.Instruction);
    return info.Offset + 5;
}

u32 Disassembler::JumpInstruction(const Chunk& chunk, const InstructionInfo& info)
{
    std::cout << std::format("[0x{:02x}] {:<20} ", info.Instruction, info.OpName);
    auto& bytes = chunk.m_Code;
    i32 jumpLen = *reinterpret_cast<const i32*>(&bytes[info.Offset + 1]);
    if (jumpLen > 0)
    {
        std::cout << std::format("[0x{:08x}] {}\n", jumpLen, jumpLen + info.Offset + 5);
    }
    else
    {
        std::cout << std::format("[0x{:08x}] {}\n", (u32)jumpLen, jumpLen + info.Offset + 5);
    }
    s_State.LastOpCode = static_cast<OpCode>(info.Instruction);
    return info.Offset + 5;
}

u32 Disassembler::ClosureInstruction(const Chunk& chunk, const InstructionInfo& info)
{
    std::cout << std::format("[0x{:02x}] {:<20} ", info.Instruction, info.OpName);
    auto& bytes = chunk.m_Code;
    u32 funIndex;
    if (s_State.LastOpCode == OpCode::OpConstant)
    {
        funIndex = chunk.m_Code[info.Offset - 1];
        std::cout << std::format("[0x{:02x}] {}\n", funIndex, chunk.m_Values[funIndex]);
    }
    else
    {
        funIndex = *reinterpret_cast<const u32*>(&bytes[info.Offset - 4]);
        std::cout << std::format("[0x{:08x}] {}\n", funIndex, chunk.m_Values[funIndex]);
    }
    ObjHandle fun = chunk.m_Values[funIndex].As<ObjHandle>();
    for (u8 i = 0; i < fun.As<FunObj>().UpvalueCount; i++)
    {
        bool isLocal = (bool)chunk.m_Code[info.Offset + 1 + i * 2];
        u8 index = chunk.m_Code[info.Offset + 2 + i * 2];
        std::cout << std::format("{:0>5}: {:>5}: {:<27} ", info.Offset + 1 + i * 2, "|", "");
        if (isLocal) std::cout << std::format("local  [0x{:02x}] {}\n", index, index);
        else std::cout << std::format("upval  [0x{:02x}] {}\n", index, index);
    }
    return info.Offset + 1 + fun.As<FunObj>().UpvalueCount * 2;
}
