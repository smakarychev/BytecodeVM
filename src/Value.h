#pragma once

#include <format>
#include <variant>

#include "Types.h"
template<class... Ts> struct ValueFormatOverload : Ts... { using Ts::operator()...; };
using Value = std::variant<bool, f64, void*>;

template <>
struct std::formatter<Value> : std::formatter<std::string> {
    auto format(Value v, format_context& ctx) {
        return std::visit(ValueFormatOverload{
            [this, &ctx](bool b) { return formatter<string>::format(std::format("{}", b ? "true" : "false"), ctx); },
            [this, &ctx](f64 f) { return formatter<string>::format(std::format("{}", f), ctx); },
            [this, &ctx](void*) { return formatter<string>::format(std::format("Nil"), ctx); },
        }, v);
    }
};