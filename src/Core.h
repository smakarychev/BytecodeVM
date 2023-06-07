#pragma once

#include "Log.h"

#define BCVM_ASSERT(x, ...) if(x) {} else { LOG_FATAL("Assertion failed"); LOG_FATAL(__VA_ARGS__); __debugbreak(); }