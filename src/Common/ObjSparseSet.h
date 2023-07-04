#pragma once
#include <vector>

#include "Types.h"
#include "Value.h"

class ObjSparseSet
{
public:
    bool Has(ObjHandle obj) const;

    const Value& Get(ObjHandle obj) const;
    Value& Get(ObjHandle obj);
    const Value& operator[](ObjHandle obj) const;
    Value& operator[](ObjHandle obj);

    void Set(ObjHandle obj, Value value);
    
private:
    std::vector<u32> m_Sparse;
    std::vector<Value> m_Dense;
    static constexpr u32 SPARSE_NONE = std::numeric_limits<u32>::max();
};
