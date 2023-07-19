#include "Obj.h"

std::vector<ObjRecord> ObjRegistry::s_Records = std::vector<ObjRecord>{};
u64 ObjRegistry::s_FreeList = FREELIST_EMPTY;

ObjHandle ObjRegistry::Clone(ObjHandle obj)
{
    switch (obj.GetType())
    {
    case ObjType::String:
        return Create<StringObj>(obj.As<StringObj>().String);
    case ObjType::Fun:
        {
            ObjHandle clone = Create<FunObj>();
            clone.As<FunObj>().Arity = obj.As<FunObj>().Arity;
            clone.As<FunObj>().UpvalueCount = obj.As<FunObj>().UpvalueCount;
            clone.As<FunObj>().Chunk = obj.As<FunObj>().Chunk;
            return clone;
        }
    case ObjType::NativeFun:
        return Create<NativeFunObj>(obj.As<NativeFunObj>().NativeFn);
    case ObjType::Closure:
        {
            ObjHandle clone = Create<ClosureObj>(obj.As<ClosureObj>().Fun);
            for (u32 i = 0; i < clone.As<ClosureObj>().UpvalueCount; i++)
            {
                clone.As<ClosureObj>().Upvalues[i] = Clone(obj.As<ClosureObj>().Upvalues[i]);
            }
            return clone;
        }
    case ObjType::Upvalue:
        {
            if (obj.As<UpvalueObj>().Location == &obj.As<UpvalueObj>().Closed)
            {
                ObjHandle clone = Create<UpvalueObj>();
                if (obj.As<UpvalueObj>().Closed.HasType<ObjHandle>())
                {
                    clone.As<UpvalueObj>().Closed = Clone(obj.As<UpvalueObj>().Closed.As<ObjHandle>());
                }
                else
                {
                    clone.As<UpvalueObj>().Closed =obj.As<UpvalueObj>().Closed;
                }
                clone.As<UpvalueObj>().Location = &clone.As<UpvalueObj>().Closed;
                return clone;
            }
            // if upvalue is open, just return it
            return obj;
        }
    case ObjType::Class:
        {
            ObjHandle clone = Create<ClassObj>(obj.As<ClassObj>().Name);
            clone.As<ClassObj>().Methods = obj.As<ClassObj>().Methods;
            return clone;
        }
    case ObjType::Instance:
        {
            ObjHandle clone = Create<InstanceObj>(obj.As<InstanceObj>().Class);
            clone.As<InstanceObj>().Fields = obj.As<InstanceObj>().Fields;
            for (auto& val : clone.As<InstanceObj>().Fields.m_Dense)
            {
                if (val.HasType<ObjHandle>())
                {
                    val = Clone(val.As<ObjHandle>());
                }
            }
            return clone;
        }
    case ObjType::BoundMethod:
        return Create<BoundMethodObj>(Clone(obj.As<BoundMethodObj>().Receiver), Clone(obj.As<BoundMethodObj>().Method));
    case ObjType::Collection:
        {
            ObjHandle clone = Create<CollectionObj>(obj.As<CollectionObj>().ItemCount);
            for (u32 i = 0; i < obj.As<CollectionObj>().ItemCount; i++)
            {
                if (obj.As<CollectionObj>().Items[i].HasType<ObjHandle>())
                {
                    clone.As<CollectionObj>().Items[i] = Clone(obj.As<CollectionObj>().Items[i].As<ObjHandle>());
                }
                else
                {
                    clone.As<CollectionObj>().Items[i] = obj.As<CollectionObj>().Items[i];
                }
            }
            return clone;
        }
    default:
        BCVM_ASSERT(false, "Something went really wrong")
        break;
    }
    std::unreachable();
}

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
    case ObjType::Class:
        GarbageCollector::GetContext().m_AllocatedBytes -= sizeof(ClassObj);
        delete static_cast<ClassObj*>(obj);
        break;
    case ObjType::Instance:
        GarbageCollector::GetContext().m_AllocatedBytes -= sizeof(InstanceObj);
        delete static_cast<InstanceObj*>(obj);
        break;
    case ObjType::BoundMethod:
        GarbageCollector::GetContext().m_AllocatedBytes -= sizeof(BoundMethodObj);
        delete static_cast<BoundMethodObj*>(obj);
        break;
    case ObjType::Collection:
        GarbageCollector::GetContext().m_AllocatedBytes -= sizeof(CollectionObj);
        delete static_cast<CollectionObj*>(obj);
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
