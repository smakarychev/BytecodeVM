#include "Obj.h"

std::vector<ObjRecord> ObjRegistry::s_Records = std::vector<ObjRecord>{};
u64 ObjRegistry::s_FreeList = FREELIST_EMPTY;

ObjType ObjHandle::GetType() const
{
    return ObjRegistry::GetType(*this);
}

ObjHandle::ObjHandle(u64 index): m_ObjIndex(index)
{
}

void ObjRegistry::DeleteObj(ObjHandle obj)
{
    u64 index = obj.m_ObjIndex;
    auto& rec = s_Records[index];
    BCVM_ASSERT(!rec.HasFlag(ObjRecord::Free), "Object already was deleted.")
    rec.AddFlag(ObjRecord::Free);
    DeleteObj(rec.Obj);
    rec.Obj = reinterpret_cast<Obj*>(s_FreeList);
    s_FreeList = index;
}

u64 ObjRegistry::PushOrReuse(ObjRecord&& record)
{
    if (s_FreeList == FREELIST_EMPTY)
    {
        usize index = s_Records.size();
        s_Records.emplace_back(record);
        return index;
    }
    u64 index = s_FreeList;
    s_FreeList = reinterpret_cast<u64>(s_Records[index].Obj);
    s_Records[index] = record;
    return index;
}

void ObjRegistry::DeleteObj(Obj* obj)
{
    switch (obj->GetType())
    {
    case ObjType::String:
        delete static_cast<StringObj*>(obj);
        break;
    case ObjType::Fun:
        delete static_cast<FunObj*>(obj);
        break;
    case ObjType::NativeFun:
        delete static_cast<NativeFunObj*>(obj);
        break;
    default:
        BCVM_ASSERT(false, "Something went really wrong")
        break;
    }
}

namespace std
{
    size_t hash<StringObj>::operator()(const StringObj& stringObj) const noexcept
    {
        return hash<std::string>{}(stringObj.String);
    }

    size_t hash<FunObj>::operator()(const FunObj& funObj) const noexcept
    {
        return hash<void*>{}((void*)&funObj.Chunk);
    }

    size_t hash<NativeFunObj>::operator()(const NativeFunObj& nativeFunObj) const noexcept
    {
        return hash<void*>{}((void*)&nativeFunObj.NativeFn);
    }

    size_t hash<ObjHandle>::operator()(ObjHandle objHandle) const noexcept
    {
        return objHandle.m_ObjIndex;
    }
}
