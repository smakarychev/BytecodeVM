﻿#pragma once

#define OBJ_TYPE(x) static constexpr ObjType GetStaticType() { return ObjType::x; }
#include <functional>
#include <string_view>

#include "Chunk.h"
#include "Core.h"
#include "Types.h"
#include "ObjHandle.h"

class Obj
{
public:
    ObjType GetType() const { return m_Type; }
    OBJ_TYPE(None)
protected:
    Obj(ObjType type) : m_Type(type) {}
    ObjType m_Type;
};

struct StringObj : Obj, ObjHasher<StringObj>
{
    OBJ_TYPE(String)
    StringObj() : Obj(ObjType::String) {}
    StringObj(std::string_view string) : Obj(ObjType::String), String(string) {}
    std::string String{};
};

struct FunObj : Obj, ObjHasher<FunObj>
{
    OBJ_TYPE(Fun)
    FunObj() : Obj(ObjType::Fun) {}
    std::string_view GetName() const { return Chunk.GetName(); }
    u32 Arity{0};
    u8 UpvalueCount{0};
    Chunk Chunk;
};

struct NativeFnCallResult
{
    Value Result{nullptr};
    bool IsOk{false};
};

using NativeFn = NativeFnCallResult (*)(u8 argc, Value* argv);

struct NativeFunObj : Obj, ObjHasher<NativeFunObj>
{
    OBJ_TYPE(NativeFun)
    NativeFunObj(NativeFn nativeFn) : Obj(ObjType::NativeFun), NativeFn(nativeFn) {}
    NativeFn NativeFn;
};

struct ClosureObj : Obj, ObjHasher<ClosureObj>
{
    OBJ_TYPE(Closure)
    ClosureObj(ObjHandle fun) : Obj(ObjType::Closure), Fun(fun)
    {
        UpvaluesCount = fun.As<FunObj>().UpvalueCount;
        Upvalues = new ObjHandle[fun.As<FunObj>().UpvalueCount]{ObjHandle::NonHandle()};
    }
    ~ClosureObj()
    {
        delete[] Upvalues;
    }
    ObjHandle Fun{ObjHandle::NonHandle()};
    ObjHandle* Upvalues{nullptr};
    u8 UpvaluesCount{0};
};

struct UpvalueObj : Obj, ObjHasher<UpvalueObj>
{
    OBJ_TYPE(Upvalue)
    UpvalueObj() : Obj(ObjType::Upvalue) {}
    Value* Location{nullptr};
    union
    {
        Value Closed{};
        u32 Index;    
    };
    ObjHandle Next{};
};

struct ObjRecord
{
    enum RecFlags
    {
        None = 0,
        Free = Bit(1)
    };
    Obj* Obj{nullptr};
    RecFlags Flags{None};
    // more goes here later (gc related stuff)

    void AddFlag(RecFlags flag)
    {
        Flags = RecFlags(Flags | flag);
    }
    bool HasFlag(RecFlags flag) const
    {
        return ((u32)Flags & flag) == flag;
    }
};

class ObjRegistry
{
public:
    template <typename T, typename ... Args>
    static ObjHandle Create(Args&&... args)
    {
        static_assert(std::is_base_of_v<Obj, T>, "Type must be derived from Obj.");
        static_assert(!std::is_same_v<Obj, T>, "Cannot create basic Obj type.");
        T* newObj = new T(std::forward<Args>(args)...);
        ObjHandle handle = PushOrReuse({ .Obj = static_cast<Obj*>(newObj) });
        return handle;
    }
    static void Delete(ObjHandle obj);
    static u64 PushOrReuse(ObjRecord&& record);
    static ObjType GetType(ObjHandle obj)
    {
        return s_Records[obj.m_ObjIndex].Obj->GetType();
    }
    template <typename T>
    static bool HasType(ObjHandle obj)
    {
        static_assert(std::is_base_of_v<Obj, T>, "Type must be derived from Obj.");
        static_assert(!std::is_same_v<Obj, T>, "Usage of base type Obj is incorrect.");
        return s_Records[obj.m_ObjIndex].Obj->GetType() == T::GetStaticType();
    }
    template <typename T>
    static T& As(ObjHandle obj)
    {
        return *Get<T>(obj);
    }
    template <typename T>
    static T* Get(ObjHandle obj)
    {
        static_assert(std::is_base_of_v<Obj, T>, "Type must be derived from Obj.");
        static_assert(!std::is_same_v<Obj, T>, "Usage of base type Obj is incorrect.");
        return static_cast<T*>(s_Records[obj.m_ObjIndex].Obj);
    }
    static void Shutdown()
    {
        for (auto& record : s_Records) Delete(record.Obj);
        s_Records.clear();
    }
private:
    static void Delete(Obj* obj);
private:
    static std::vector<ObjRecord> s_Records;
    static constexpr u64 FREELIST_EMPTY = std::numeric_limits<u64>::max();
    static u64 s_FreeList; 
};

namespace std
{
    template<>
    struct hash<StringObj>
    {
        size_t operator()(const StringObj& stringObj) const noexcept;
    };

    template<>
    struct hash<FunObj>
    {
        size_t operator()(const FunObj& funObj) const noexcept;
    };

    template<>
    struct hash<NativeFunObj>
    {
        size_t operator()(const NativeFunObj& nativeFunObj) const noexcept;
    };

    template<>
    struct hash<ObjHandle>
    {
        size_t operator()(ObjHandle objHandle) const noexcept;
    };
}