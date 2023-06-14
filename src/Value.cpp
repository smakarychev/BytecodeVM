#include "Value.h"

std::vector<ObjRecord> ObjRegistry::s_Records = std::vector<ObjRecord>{};

ObjHash::ObjHash(u64 hash): m_Hash(hash)
{
}

ObjHandle::ObjHandle(u64 index): m_ObjIndex(index)
{}

namespace std
{
    size_t hash<ObjHash>::operator()(ObjHash objHash) const noexcept
    {
        return objHash.m_Hash;
    }

    size_t hash<StringObj>::operator()(const StringObj& stringObj) const noexcept
    {
        return hash<std::string>{}(stringObj.String);
    }
}


