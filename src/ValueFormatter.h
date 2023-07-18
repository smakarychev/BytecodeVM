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
        case ObjType::Upvalue: return formatter<string>::format(std::format("Upvalue 0x{:016x} {}", (u64)obj.As<UpvalueObj>().Location, obj.As<UpvalueObj>().Index), ctx);
        case ObjType::Class: return formatter<string>::format(std::format("Class {}", obj.As<ClassObj>().Name.As<StringObj>().String), ctx);
        case ObjType::Instance: return formatter<string>::format(std::format("Instance of {}", obj.As<InstanceObj>().Class.As<ClassObj>().Name.As<StringObj>().String), ctx);
        case ObjType::BoundMethod: return formatter<string>::format(std::format("BoundMethod {}", obj.As<BoundMethodObj>().Method.As<ClosureObj>().Fun.As<FunObj>().GetName()), ctx);
        case ObjType::Collection: return formatter<string>::format(std::format("Collection {}", obj.As<CollectionObj>().ItemCount), ctx);
        default: break;
        }
        BCVM_ASSERT(false, "Unrecognized Obj type.")
        std::unreachable();
    }
};

template <>
struct std::formatter<Value> : std::formatter<ObjHandle>
{
    auto format(Value v, format_context& ctx){
#ifdef NAN_BOXING
        if (v.HasType<bool>())      return formatter<string>::format(std::format("{}", v.As<bool>() ? "true" : "false"), ctx);
        if (v.HasType<f64>())       return formatter<string>::format(std::format("{}", v.As<f64>()), ctx);
        if (v.HasType<void*>())     return formatter<string>::format(std::format("Nil"), ctx);
        if (v.HasType<ObjHandle>()) return formatter<ObjHandle>::format(v.As<ObjHandle>(), ctx);
#else
        switch (v.GetType())
        {
        case ValueType::Bool: return formatter<string>::format(std::format("{}", v.As<bool>() ? "true" : "false"), ctx);
        case ValueType::F64: return formatter<string>::format(std::format("{}", v.As<f64>()), ctx);
        case ValueType::Nil: return formatter<string>::format(std::format("Nil"), ctx);
        case ValueType::Obj: return formatter<ObjHandle>::format(v.As<ObjHandle>(), ctx);
        }
#endif
        BCVM_ASSERT(false, "Unrecognized value type.")
        unreachable();
    }
};
