#pragma once

#include <format>
#include <variant>
#include <vector>

#include "Types.h"

template<class... Ts> struct ValueFormatOverload : Ts... { using Ts::operator()...; };

#define OBJ_TYPE(x) static ObjType GetStaticType() { return ObjType::x; }

enum class ObjType
{
    None = 0,
    String,
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

class ObjHash;

namespace std
{
    template <>
    struct hash<ObjHash>
    {
        size_t operator()(ObjHash objHash) const noexcept;
    };    
}

class ObjHash
{
    friend struct std::hash<ObjHash>;
public:
    ObjHash(u64 hash);
private:
    u64 m_Hash;
};

template <typename ObjImpl>
struct ObjHasher
{
    ObjHash Hash() { return ObjHash{std::hash<ObjImpl>{}(static_cast<ObjImpl&>(*this))}; }
};

struct StringObj : Obj, ObjHasher<StringObj>
{
    OBJ_TYPE(String)
    StringObj() : Obj(ObjType::String) {}
    StringObj(std::string_view string) : Obj(ObjType::String), String(string) {}
    std::string String{};
};

struct ObjRecord
{
    Obj* Obj;
    ObjHash Hash;
    // more goes here later (gc related stuff)
};

class ObjHandle
{
    friend class ObjRegistry;
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
        ObjHash hash = newObj->Hash();
        ObjHandle handle = s_Records.size();
        s_Records.push_back({ .Obj = static_cast<Obj*>(newObj), .Hash = hash });
        return handle;
    }
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
        static_assert(std::is_base_of_v<Obj, T>, "Type must be derived from Obj.");
        static_assert(!std::is_same_v<Obj, T>, "Usage of base type Obj is incorrect.");
        return static_cast<T&>(*s_Records[obj.m_ObjIndex].Obj);
    }
    static void Shutdown()
    {
        for (auto& record : s_Records) delete record.Obj;
        s_Records.clear();
    }
private:
    static std::vector<ObjRecord> s_Records;
};

using Value = std::variant<bool, f64, void*, ObjHandle>;

namespace std
{
    template<>
    struct hash<StringObj>
    {
        size_t operator()(const StringObj& stringObj) const noexcept;
    };
}


template <>
struct std::formatter<Value> : std::formatter<std::string> {
    auto format(Value v, format_context& ctx) {
        return std::visit(ValueFormatOverload{
            [this, &ctx](bool b) { return formatter<string>::format(std::format("{}", b ? "true" : "false"), ctx); },
            [this, &ctx](f64 f) { return formatter<string>::format(std::format("{}", f), ctx); },
            [this, &ctx](void*) { return formatter<string>::format(std::format("Nil"), ctx); },
            [this, &ctx](ObjHandle obj)
            {
                switch (ObjRegistry::GetType(obj))
                {
                case ObjType::String: return formatter<string>::format(
                    std::format("StringObj: \"{}\"", ObjRegistry::As<StringObj>(obj).String), ctx);
                }
                return formatter<string>::format(std::format("Obj"), ctx);
            }, 
        }, v);
    }
};