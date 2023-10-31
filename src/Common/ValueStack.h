#pragma once

#include "Value.h"
#include "Types.h"

class ValueStack
{
    using OnResizeCallback = void (*)(VirtualMachine* vm, Value* oldMem, Value* newMem);
public:
    ValueStack();

    void Emplace(Value&& val);
    void EmplaceAtTop(Value&& val);
    void Push(Value val);
    void Pop() noexcept;
    
    Value& Top();
    const Value& Top() const;
    Value& operator[](usize index);
    const Value& operator[](usize index) const;

    Value& Peek(usize delta);
    const Value& Peek(usize delta) const;

    usize GetTop() const;
    void SetTop(usize top);
    void ShiftTop(usize delta);

    Value* begin();
    Value* end();
    
    void Clear();

    void SetVirtualMachine(VirtualMachine* vm);
    void SetOnResizeCallback(OnResizeCallback callback);
    
private:
    void Init();
    void Resize(usize oldSize, usize newSize);
private:
    Value* m_DataStart{nullptr};
    Value* m_DataEnd{nullptr};
    Value* m_DataCurrent{nullptr};

    OnResizeCallback m_OnResizeCallback = [](VirtualMachine*, Value*, Value*){};
    VirtualMachine* m_VirtualMachine{nullptr};
    
    static constexpr usize DEFAULT_SIZE_ELEMENTS = 256;
    static constexpr usize SIZE_MULTIPLIER = 4;
};

inline ValueStack::ValueStack()
{
    Init();
}

inline void ValueStack::Init()
{
    m_DataStart = static_cast<Value*>(malloc(sizeof(Value) * DEFAULT_SIZE_ELEMENTS));
    m_DataCurrent = m_DataStart;
    m_DataEnd = m_DataStart + DEFAULT_SIZE_ELEMENTS;
}

inline void ValueStack::Resize(usize oldSize, usize newSize)
{
    Value* newData = static_cast<Value*>(malloc(sizeof (Value) * newSize));
    std::memcpy(newData, m_DataStart, sizeof (Value) * oldSize);
    
    m_OnResizeCallback(m_VirtualMachine, m_DataStart, newData);
    
    free(m_DataStart);
    m_DataStart = newData;
    m_DataCurrent = m_DataStart + oldSize;
    m_DataEnd = m_DataStart + newSize;
}

inline void ValueStack::Emplace(Value&& val)
{
    if (m_DataCurrent == m_DataEnd)
    {
        usize size = m_DataEnd - m_DataStart;
        Resize(size, size * SIZE_MULTIPLIER);
    }
    std::construct_at(m_DataCurrent, std::forward<Value>(val));
    m_DataCurrent++;
}

inline void ValueStack::EmplaceAtTop(Value&& val)
{
    std::construct_at(m_DataCurrent - 1, std::forward<Value>(val));
}

inline void ValueStack::Push(Value val)
{
    if (m_DataCurrent == m_DataEnd)
    {
        usize size = m_DataEnd - m_DataStart;
        Resize(size, size * SIZE_MULTIPLIER);
    }
    *m_DataCurrent = val;
    m_DataCurrent++;
}

inline void ValueStack::Pop() noexcept
{
    --m_DataCurrent;
}

inline const Value& ValueStack::Top() const
{
    return m_DataCurrent[-1];
}

inline Value& ValueStack::Top()
{
    return m_DataCurrent[-1];
}

inline const Value& ValueStack::operator[](usize index) const
{
    return m_DataStart[index];
}

inline Value& ValueStack::Peek(usize delta)
{
    return m_DataCurrent[- 1 - delta ];
}

inline const Value& ValueStack::Peek(usize delta) const
{
    return m_DataCurrent[- 1 - delta];
}

inline Value& ValueStack::operator[](usize index)
{
    return m_DataStart[index];
}

inline usize ValueStack::GetTop() const
{
    return m_DataCurrent - m_DataStart;
}

inline void ValueStack::SetTop(usize top)
{
    m_DataCurrent = m_DataStart + top;
}

inline void ValueStack::ShiftTop(usize delta)
{
    m_DataCurrent -= delta;
}

inline Value* ValueStack::begin()
{
    return m_DataStart;
}

inline Value* ValueStack::end()
{
    return m_DataCurrent;
}

inline void ValueStack::Clear()
{
    m_DataCurrent = m_DataStart;
}

inline void ValueStack::SetVirtualMachine(VirtualMachine* vm)
{
    m_VirtualMachine = vm;
}

inline void ValueStack::SetOnResizeCallback(OnResizeCallback callback)
{
    m_OnResizeCallback = callback;
}

