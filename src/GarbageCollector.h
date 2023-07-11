#pragma once
#include <vector>

#include "Types.h"

class ObjSparseSet;
class Compiler;
class VirtualMachine;
class ObjHandle;

class GCContext
{
    friend class GarbageCollector;
    friend class ObjRegistry;
public:
    VirtualMachine* VM{nullptr};
    Compiler* Compiler{nullptr};
private:
    std::vector<ObjHandle> m_GreyFuns;
    std::vector<ObjHandle> m_GreyClosures;
    std::vector<ObjHandle> m_GreyUpvalues;
    std::vector<ObjHandle> m_GreyClasses;
    std::vector<ObjHandle> m_GreyInstances;
    std::vector<ObjHandle> m_GreyBoundMethods;

    u64 m_AllocatedBytes{0};
    u64 m_AllocatedThreshold{THRESHOLD_VAL_DEFAULT};
    f64 m_ThresholdScale{THRESHOLD_SCALE_DEFAULT};

    static constexpr u64 THRESHOLD_VAL_DEFAULT{1llu * 1024 * 1024};
    static constexpr f64 THRESHOLD_SCALE_DEFAULT{2.0};
};

class GarbageCollector
{
    friend class ObjRegistry;
public:
    static void Collect();
    static void ForceCollect();
    static void InitContext(const GCContext& ctx);
    static GCContext& GetContext();
private:
    static void Mark(GCContext& ctx);
    static void MarkVMRoots(GCContext& ctx);
    static void MarkCompilerRoots(GCContext& ctx);
    static void Blacken(GCContext& ctx);

    static void MarkSparseSet(const ObjSparseSet& set, GCContext& ctx);
    static void MarkObj(ObjHandle obj, GCContext& ctx);

    static void SweepInternStrings(GCContext& ctx);
    
    static void Sweep(GCContext& ctx);
private:
    static u32 s_MarkFlag;
    static GCContext s_Context;
    static constexpr u32 MARK_FLAG_INITIAL = 1;
};
