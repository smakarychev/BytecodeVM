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
    OpReadProperty, OpReadProperty32,
    OpSetProperty,  OpSetProperty32,
    OpReadUpvalue, 
    OpSetUpvalue,  
    OpJump,
    OpJumpFalse,
    OpJumpTrue,
    OpCall,
    OpInvoke,
    OpClosure,
    OpCloseUpvalue,
    OpClass,
    OpMethod,
    OpReturn,
};
