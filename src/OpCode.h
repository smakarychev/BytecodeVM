#pragma once
#include "Types.h"

enum class OpCode : u8
{
    OpConstant,     OpConstant32,
    OpNil,
    OpFalse,        OpTrue,
    OpNegate,
    OpNot,
    OpAdd,          OpSubtract,         OpMultiply,     OpDivide,
    OpEqual,        OpLess,             OpLequal,
    OpPrint,
    OpPop,          OpPopN,
    OpDefineGlobal, OpDefineGlobal32,
    OpReadGlobal,   OpReadGlobal32,
    OpSetGlobal,    OpSetGlobal32,
    OpReadLocal,    OpReadLocal32,
    OpSetLocal,     OpSetLocal32,
    OpJump,
    OpJumpFalse,
    OpJumpTrue,
    OpCall,
    OpReturn,
};
