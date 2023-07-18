#pragma once

#include "Log.h"

#define BCVM_ASSERT(x, ...) if(x) {} else { LOG_FATAL("Assertion failed"); LOG_FATAL(__VA_ARGS__); __debugbreak(); }

#define CHECK_RETURN(x, ...) if (x) {} else { LOG_ERROR(__VA_ARGS__); return; }
#define CHECK_RETURN_RES(x, res, ...) if (x) {} else { LOG_ERROR(__VA_ARGS__); return res; }

#define Bit(x) (1 << (x))