#pragma once
#include <vector>

#include "Types.h"
#include "Value.h"

class ObjSparseSet
{
    friend class GarbageCollector;
    friend class ObjRegistry;
    friend class VirtualMachine;
public:
    bool Has(ObjHandle obj) const;

    const Value& Get(ObjHandle obj) const;
    Value& Get(ObjHandle obj);
    const Value& operator[](ObjHandle obj) const;
    Value& operator[](ObjHandle obj);

    void Set(ObjHandle obj, Value value);
private:
    ObjHandle GetKey(u64 index);
    const Value& GetValue(u64 index);
private:
    std::vector<u64> m_Sparse;
    std::vector<Value> m_Dense;
    static constexpr u64 SPARSE_NONE = std::numeric_limits<u64>::max();
};

inline bool ObjSparseSet::Has(ObjHandle obj) const
{
    return obj.m_ObjIndex < m_Sparse.size() && m_Sparse[obj.m_ObjIndex] != SPARSE_NONE;
}

inline const Value& ObjSparseSet::Get(ObjHandle obj) const
{
    return m_Dense[m_Sparse[obj.m_ObjIndex]];
}

inline Value& ObjSparseSet::Get(ObjHandle obj)
{
    return const_cast<Value&>(const_cast<const ObjSparseSet&>(*this).Get(obj));
}

inline const Value& ObjSparseSet::operator[](ObjHandle obj) const
{
    return Get(obj);
}

inline Value& ObjSparseSet::operator[](ObjHandle obj)
{
    return Get(obj);
}

inline void ObjSparseSet::Set(ObjHandle obj, Value value)
{
    if (m_Sparse.size() <= obj.m_ObjIndex)
    {
        m_Sparse.resize(obj.m_ObjIndex + 1, SPARSE_NONE);
    }
    if (m_Sparse[obj.m_ObjIndex] != SPARSE_NONE)
    {
        m_Dense[m_Sparse[obj.m_ObjIndex]] = value;
    }
    else 
    {
        m_Sparse[obj.m_ObjIndex] = m_Dense.size();
        m_Dense.push_back(value);
    }
}

inline ObjHandle ObjSparseSet::GetKey(u64 index)
{
    return ObjHandle{index};
}

inline const Value& ObjSparseSet::GetValue(u64 index)
{
    return m_Dense[m_Sparse[index]];
}
