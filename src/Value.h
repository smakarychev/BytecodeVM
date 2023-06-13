#pragma once

#include <format>
#include <variant>
#include <vector>

#include "Types.h"
template<class... Ts> struct ValueFormatOverload : Ts... { using Ts::operator()...; };

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
protected:
    Obj(ObjType type) : m_Type(type) {}
    ObjType m_Type;
};

struct StringObj : public Obj
{
    StringObj() : Obj(ObjType::String) {}
    StringObj(std::string_view string) : Obj(ObjType::String), String(string) {}
    std::string String{};
};

struct ObjRecord
{
    Obj* Obj;
    // more goes here later (gc related stuff)
};

class ObjFactory
{
public:
    template <typename T, typename ... Args>
    static T* CreateObj(Args&&... args)
    {
        static_assert(std::is_base_of_v<Obj, T>, "Type must be derived from Obj.");
        T* newObj = new T(std::forward<Args>(args)...);
        s_Records.push_back({ .Obj = static_cast<Obj*>(newObj) });
        return newObj;
    }
    static void Shutdown()
    {
        for (auto& record : s_Records) delete record.Obj;
        s_Records.clear();
    }
private:
    static std::vector<ObjRecord> s_Records;
};

using Value = std::variant<bool, f64, void*, Obj*>;

template <>
struct std::formatter<Value> : std::formatter<std::string> {
    auto format(Value v, format_context& ctx) {
        return std::visit(ValueFormatOverload{
            [this, &ctx](bool b) { return formatter<string>::format(std::format("{}", b ? "true" : "false"), ctx); },
            [this, &ctx](f64 f) { return formatter<string>::format(std::format("{}", f), ctx); },
            [this, &ctx](void*) { return formatter<string>::format(std::format("Nil"), ctx); },
            [this, &ctx](Obj* obj)
            {
                switch (obj->GetType())
                {
                case ObjType::String: return formatter<string>::format(
                    std::format("StringObj: \"{}\"", static_cast<StringObj*>(obj)->String), ctx);
                }
                return formatter<string>::format(std::format("Obj"), ctx);
            }, 
        }, v);
    }
};