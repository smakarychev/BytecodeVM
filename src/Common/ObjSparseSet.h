#pragma once
#include <vector>

#include "Types.h"
#include "Value.h"

class ObjSparseSet
{
    friend class GarbageCollector;
public:
    bool Has(ObjHandle obj) const;

    const Value& Get(ObjHandle obj) const;
    Value& Get(ObjHandle obj);
    const Value& operator[](ObjHandle obj) const;
    Value& operator[](ObjHandle obj);

    void Set(ObjHandle obj, Value value);
    
private:
    std::vector<u64> m_Sparse;
    std::vector<Value> m_Dense;
    static constexpr u64 SPARSE_NONE = std::numeric_limits<u64>::max();
};
