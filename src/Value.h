#pragma once

#include <variant>

#include "Types.h"

class ObjHandle;

using Value = std::variant<bool, f64, u64, void*, ObjHandle>;