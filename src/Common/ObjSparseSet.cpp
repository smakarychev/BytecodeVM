#include "ObjSparseSet.h"

#include "ValueFormatter.h"

bool ObjSparseSet::Has(ObjHandle obj) const
{
    return obj.m_ObjIndex < m_Sparse.size() && m_Sparse[obj.m_ObjIndex] != SPARSE_NONE;
}

const Value& ObjSparseSet::Get(ObjHandle obj) const
{
    return m_Dense[m_Sparse[obj.m_ObjIndex]];
}

Value& ObjSparseSet::Get(ObjHandle obj)
{
    return const_cast<Value&>(const_cast<const ObjSparseSet&>(*this).Get(obj));
}

const Value& ObjSparseSet::operator[](ObjHandle obj) const
{
    return Get(obj);
}

Value& ObjSparseSet::operator[](ObjHandle obj)
{
    return Get(obj);
}

void ObjSparseSet::Set(ObjHandle obj, Value value)
{
    if (Has(obj))
    {
        m_Dense[m_Sparse[obj.m_ObjIndex]] = value;
    }
    else
    {
        m_Sparse.resize(obj.m_ObjIndex + 1, SPARSE_NONE);
        m_Sparse[obj.m_ObjIndex] = m_Dense.size();
        m_Dense.push_back(value);
    }
}
