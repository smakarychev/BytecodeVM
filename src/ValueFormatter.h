#pragma once
#include "Value.h"
#include "Obj.h"

template <>
struct std::formatter<ObjHandle> : std::formatter<std::string>
{
    auto format(ObjHandle obj, format_context& ctx)
    {
        switch (obj.GetType())
        {
        case ObjType::String: return formatter<string>::format(std::format("{}", obj.As<StringObj>().String), ctx);
        case ObjType::Fun: return formatter<string>::format(std::format("FunObj {}", obj.As<FunObj>().GetName()), ctx);
        case ObjType::NativeFun: return formatter<string>::format(std::format("NativeFunObj {}", (void*)obj.As<NativeFunObj>().NativeFn), ctx);
        case ObjType::Closure: return formatter<string>::format(std::format("ClosureObj {}", obj.As<ClosureObj>().Fun.As<FunObj>().GetName()), ctx);
        case ObjType::Upvalue: return formatter<string>::format(std::format("Upvalue {}", *obj.As<UpvalueObj>().Location), ctx);
        default: break;
        }
        return formatter<string>::format(std::format("Obj"), ctx);
    }
};

template <>
struct std::formatter<Value> : std::formatter<ObjHandle>
{
    auto format(Value v, format_context& ctx){
        switch (v.GetType())
        {
        case ValueType::Bool: return formatter<string>::format(std::format("{}", v.As<bool>() ? "true" : "false"), ctx);
        case ValueType::F64: return formatter<string>::format(std::format("{}", v.As<f64>()), ctx);
        case ValueType::U64: return formatter<string>::format(std::format("{}", v.As<u64>()), ctx);
        case ValueType::Nil: return formatter<string>::format(std::format("Nil"), ctx);
        case ValueType::Obj: return formatter<ObjHandle>::format(v.As<ObjHandle>(), ctx);
        }
        BCVM_ASSERT(false, "Unknown value type.")
        unreachable();
    }
};
