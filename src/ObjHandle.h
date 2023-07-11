#pragma once
#include <functional>

#include "Types.h"

enum class ObjType
{
    None = 0,
    String,
    Fun,
    NativeFun,
    Closure,
    Upvalue,
    Class,
    Instance,
    BoundMethod,
    Count
};

template <typename ObjImpl>
struct ObjHasher
{
    constexpr ObjHasher() = default;
    u64 Hash() { return std::hash<ObjImpl>{}(static_cast<ObjImpl&>(*this)); }
    friend auto operator<=>(const ObjHasher&, const ObjHasher&) = default;
};

class ObjHandle : ObjHasher<ObjHandle>
{
    friend class ObjRegistry;
    friend class ObjSparseSet;
    friend class GarbageCollector;
    friend struct std::hash<ObjHandle>;
public:
    static constexpr ObjHandle NonHandle() { return ObjHandle{std::numeric_limits<u64>::max()}; }
    constexpr ObjHandle() = default;
    
    ObjType GetType() const;

    template <typename T>
    bool HasType() const;
    
    template <typename T>
    T& As() const;

    template <typename T>
    T* Get() const;
    friend auto operator<=>(const ObjHandle&, const ObjHandle&) = default;
private:
    constexpr ObjHandle(u64 index): m_ObjIndex(index) {}
private:
    u64 m_ObjIndex{std::numeric_limits<u64>::max()};
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