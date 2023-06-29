#pragma once
#include "Value.h"
#include "Obj.h"

template<class... Ts> struct ValueFormatOverload : Ts... { using Ts::operator()...; };

template <>
struct std::formatter<Value> : std::formatter<std::string>
{
    auto format(Value v, format_context& ctx){
        return std::visit(ValueFormatOverload{
              [this, &ctx](bool b) { return formatter<string>::format(std::format("{}", b ? "true" : "false"), ctx); },
              [this, &ctx](f64 f) {  return formatter<string>::format(std::format("{}", f), ctx); },
              [this, &ctx](u64 u) {  return formatter<string>::format(std::format("{}", u), ctx); },
              [this, &ctx](void*) {  return formatter<string>::format(std::format("Nil"), ctx); },
              [this, &ctx](ObjHandle obj)
              {
                  switch (ObjRegistry::GetType(obj))
                  {
                  case ObjType::String: return formatter<string>::format(std::format("{}", obj.As<StringObj>().String), ctx);
                  case ObjType::Fun: return formatter<string>::format(std::format("FunObj {}", obj.As<FunObj>().GetName()), ctx);
                  }
                  return formatter<string>::format(std::format("Obj"), ctx);
              },
        }, v);
    }
    
};
