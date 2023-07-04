#include "ObjHandle.h"

#include "Obj.h"

ObjType ObjHandle::GetType() const
{
    return ObjRegistry::GetType(*this);
}

namespace std
{
    size_t hash<ObjHandle>::operator()(ObjHandle objHandle) const noexcept
    {
        return objHandle.m_ObjIndex;
    }
}