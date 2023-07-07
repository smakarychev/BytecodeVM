#include "GarbageCollector.h"

#include "Compiler.h"
#include "VirtualMachine.h"
#include "ValueFormatter.h"

u32 GarbageCollector::s_MarkFlag = MARK_FLAG_INITIAL;
GCContext GarbageCollector::s_Context = GCContext{};

void GarbageCollector::Collect()
{
#ifdef GC_STRESS_TEST
    ForceCollect();
    return;
#endif
    if (s_Context.m_AllocatedBytes > s_Context.m_AllocatedThreshold)
    {
        ForceCollect();
        s_Context.m_AllocatedThreshold = std::max(
            GCContext::THRESHOLD_VAL_DEFAULT,
            (u64)((f64)s_Context.m_AllocatedBytes * s_Context.m_ThresholdScale));
    }
}

void GarbageCollector::ForceCollect()
{
    Mark(s_Context);
    SweepInternStrings(s_Context);
    Sweep(s_Context);
    s_MarkFlag ^= 1;
}

void GarbageCollector::InitContext(const GCContext& ctx)
{
    s_Context = ctx;
}

GCContext& GarbageCollector::GetContext()
{
    return s_Context;
}

void GarbageCollector::Mark(GCContext& ctx)
{
    MarkVMRoots(ctx);
    MarkCompilerRoots(ctx);
    Blacken(ctx); 
}

void GarbageCollector::MarkVMRoots(GCContext& ctx)
{
    // mark stack
#ifdef DEBUG_TRACE
    LOG_INFO("GC::Mark::VM::Stack");
#endif
    std::vector<Value>& valueStack = ctx.VM->m_ValueStack;
    for (auto& val : valueStack)
    {
        if (val.HasType<ObjHandle>()) MarkObj(val.As<ObjHandle>(), ctx);
    }
    // mark callstack
#ifdef DEBUG_TRACE
    LOG_INFO("GC::Mark::VM::CallStack");
#endif
    std::vector<CallFrame>& callStack = ctx.VM->m_CallFrames;
    for (auto& frame : callStack)
    {
        MarkObj(frame.Fun, ctx);
        MarkObj(frame.Closure, ctx);
    }
    // mark globals
#ifdef DEBUG_TRACE
    LOG_INFO("GC::Mark::VM::Globals");
#endif
    ObjSparseSet& globals = ctx.VM->m_GlobalsSparseSet;
    for (auto& globalVal : globals.m_Dense)
    {
        if (globalVal.HasType<ObjHandle>()) MarkObj(globalVal.As<ObjHandle>(), ctx);
    }
    for (auto& globalKey : globals.m_Sparse)
    {
        if (globalKey != ObjSparseSet::SPARSE_NONE) MarkObj(ObjHandle(globalKey), ctx);
    }
}

void GarbageCollector::MarkCompilerRoots(GCContext& ctx)
{
    if (ctx.Compiler == nullptr) return;
#ifdef DEBUG_TRACE
        LOG_INFO("GC::Mark::Compiler");
#endif
    for (CompilerContext* cContext = &ctx.Compiler->m_CurrentContext; cContext != nullptr; cContext = cContext->Previous)
    {
        MarkObj(cContext->Fun, ctx);
    }
}

void GarbageCollector::Blacken(GCContext& ctx)
{
    // each of grey objects might add new grey objects
    for (;;)
    {
        while (!ctx.m_GreyFuns.empty())
        {
#ifdef DEBUG_TRACE
            LOG_INFO("GC::Blacken: {}", ctx.m_GreyFuns.back());
#endif
            FunObj& fun = ctx.m_GreyFuns.back().As<FunObj>(); ctx.m_GreyFuns.pop_back();
            for (auto& val : fun.Chunk.m_Values)
            {
                if (val.HasType<ObjHandle>()) MarkObj(val.As<ObjHandle>(), ctx);            
            }
        }

        while (!ctx.m_GreyClosures.empty())
        {
#ifdef DEBUG_TRACE
            LOG_INFO("GC::Blacken: {}", ctx.m_GreyClosures.back());
#endif
            ClosureObj& closure = ctx.m_GreyClosures.back().As<ClosureObj>(); ctx.m_GreyClosures.pop_back();
            MarkObj(closure.Fun, ctx);
            for (u32 i = 0; i < closure.UpvaluesCount; i++) MarkObj(closure.Upvalues[i], ctx);
        }

        while (!ctx.m_GreyUpvalues.empty())
        {
#ifdef DEBUG_TRACE
            LOG_INFO("GC::Blacken: {}", ctx.m_GreyUpvalues.back());
#endif
            UpvalueObj& upvalue = ctx.m_GreyUpvalues.back().As<UpvalueObj>(); ctx.m_GreyUpvalues.pop_back();
            if (upvalue.Location == &upvalue.Closed && upvalue.Closed.HasType<ObjHandle>()) MarkObj(upvalue.Closed.As<ObjHandle>(), ctx);
        }

        if (ctx.m_GreyFuns.empty() && ctx.m_GreyClosures.empty() && ctx.m_GreyUpvalues.empty()) break;
    }
}

void GarbageCollector::MarkObj(ObjHandle obj, GCContext& ctx)
{
    if (obj == ObjHandle::NonHandle()) return;
    if (ObjRegistry::s_Records[obj.m_ObjIndex].MarkFlag == s_MarkFlag) return;
#ifdef DEBUG_TRACE
    LOG_INFO("GC::Mark: {}", obj);
#endif
    ObjRegistry::s_Records[obj.m_ObjIndex].MarkFlag = s_MarkFlag;
    // if obj can point to other objects, mark it as grey
    if (obj.HasType<FunObj>()) ctx.m_GreyFuns.push_back(obj);
    else if (obj.HasType<ClosureObj>()) ctx.m_GreyClosures.push_back(obj);
    else if (obj.HasType<UpvalueObj>()) ctx.m_GreyUpvalues.push_back(obj);
}

void GarbageCollector::SweepInternStrings(GCContext& ctx)
{
    for (auto it = ctx.VM->m_InternedStrings.cbegin(); it != ctx.VM->m_InternedStrings.cend();)
    {
        if (ObjRegistry::s_Records[it->second.m_ObjIndex].MarkFlag != s_MarkFlag)
        {
#ifdef DEBUG_TRACE
            LOG_INFO("GC::Delete::InternKey: {}", it->second);
#endif
            it = ctx.VM->m_InternedStrings.erase(it);
        }
        else
        {
            ++it;
        }
    }
}

void GarbageCollector::Sweep(GCContext& ctx)
{
    for (u32 i = 0; i < ObjRegistry::s_Records.size(); i++)
    {
        auto& record = ObjRegistry::s_Records[i];
        if (record.MarkFlag == (s_MarkFlag ^ 1))
        {
#ifdef DEBUG_TRACE
            LOG_INFO("GC::Delete: {}", ObjHandle(i));
#endif
            ObjRegistry::Delete(ObjHandle(i));
        }
    }
}
