#include "Obj.h"

std::vector<ObjRecord> ObjRegistry::s_Records = std::vector<ObjRecord>{};
u64 ObjRegistry::s_FreeList = FREELIST_EMPTY;


void ObjRegistry::Delete(ObjHandle obj)
{
    u64 index = obj.m_ObjIndex;
    ObjRecord& rec = s_Records[index];
    rec.MarkFlag = ObjRecord::DELETED_FLAG;
    Delete(rec.Obj);
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

void ObjRegistry::Delete(Obj* obj)
{
    switch (obj->GetType())
    {
    case ObjType::String:
        GarbageCollector::GetContext().m_AllocatedBytes -= sizeof(StringObj);
        delete static_cast<StringObj*>(obj);
        break;
    case ObjType::Fun:
        GarbageCollector::GetContext().m_AllocatedBytes -= sizeof(FunObj);
        delete static_cast<FunObj*>(obj);
        break;
    case ObjType::NativeFun:
        GarbageCollector::GetContext().m_AllocatedBytes -= sizeof(NativeFunObj);
        delete static_cast<NativeFunObj*>(obj);
        break;
    case ObjType::Closure:
        GarbageCollector::GetContext().m_AllocatedBytes -= sizeof(ClosureObj);
        delete static_cast<ClosureObj*>(obj);
        break;
    case ObjType::Upvalue:
        GarbageCollector::GetContext().m_AllocatedBytes -= sizeof(UpvalueObj);
        delete static_cast<UpvalueObj*>(obj);
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
}
