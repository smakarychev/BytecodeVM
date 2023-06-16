#pragma once
#include "Types.h"

enum class OpCode : u8
{
    OpConstant,
    OpConstant24,
    OpNil,
    OpFalse,
    OpTrue,
    OpNegate,
    OpNot,
    OpAdd, OpSubtract, OpMultiply, OpDivide,
    OpEqual, OpLess, OpLequal,
    OpPrint,
    OpPop,
    OpDefineGlobal,
    OpReadGlobal,
    OpSetGlobal,
    OpReturn,
};
