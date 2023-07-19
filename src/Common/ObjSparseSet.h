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
