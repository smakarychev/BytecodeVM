#pragma once

#define OBJ_TYPE(x) static ObjType GetStaticType() { return ObjType::x; }
#include <functional>
#include <string_view>

#include "Chunk.h"
#include "Core.h"
#include "Types.h"

enum class ObjType
{
    None = 0,
    String,
    Fun,
    Count
};

class Obj
{
public:
    ObjType GetType() const { return m_Type; }
    OBJ_TYPE(None)
protected:
    Obj(ObjType type) : m_Type(type) {}
    ObjType m_Type;
};

template <typename ObjImpl>
struct ObjHasher
{
    u64 Hash() { return std::hash<ObjImpl>{}(static_cast<ObjImpl&>(*this)); }
    friend auto operator<=>(const ObjHasher&, const ObjHasher&) = default;
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
    Chunk Chunk;
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

class ObjHandle : ObjHasher<ObjHandle>
{
    friend class ObjRegistry;
    friend struct std::hash<ObjHandle>;
public:
    static ObjHandle NonHandle() { return ObjHandle{std::numeric_limits<u64>::max()}; } 
    
    ObjType GetType() const;

    template <typename T>
    bool HasType() const;
    
    template <typename T>
    T& As() const;

    template <typename T>
    T* Get() const;
    friend auto operator<=>(const ObjHandle&, const ObjHandle&) = default;
private:
    ObjHandle(u64 index);
private:
    u64 m_ObjIndex{std::numeric_limits<u64>::max()};
};

class ObjRegistry
{
public:
    template <typename T, typename ... Args>
    static ObjHandle CreateObj(Args&&... args)
    {
        static_assert(std::is_base_of_v<Obj, T>, "Type must be derived from Obj.");
        static_assert(!std::is_same_v<Obj, T>, "Cannot create basic Obj type.");
        T* newObj = new T(std::forward<Args>(args)...);
        ObjHandle handle = PushOrReuse({ .Obj = static_cast<Obj*>(newObj) });
        return handle;
    }
    static void DeleteObj(ObjHandle obj);
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
        for (auto& record : s_Records) DeleteObj(record.Obj);
        s_Records.clear();
    }
private:
    static void DeleteObj(Obj* obj);
private:
    static std::vector<ObjRecord> s_Records;
    static constexpr u64 FREELIST_EMPTY = std::numeric_limits<u64>::max();
    static u64 s_FreeList; 
};

template <typename T>
bool ObjHandle::HasType() const
{
    return ObjRegistry::HasType<T>(*this);
}

template <typename T>
T& ObjHandle::As() const
{
    return ObjRegistry::As<T>(*this);
}

template <typename T>
T* ObjHandle::Get() const
{
    return ObjRegistry::Get<T>(*this);
}

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
    struct hash<ObjHandle>
    {
        size_t operator()(ObjHandle objHandle) const noexcept;
    };
}