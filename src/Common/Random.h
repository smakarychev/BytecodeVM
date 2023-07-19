#pragma once
#include <random>

#include "Types.h"

class Random
{
public:
    static f64 F64();
private:
    static std::random_device m_Device;
    static std::mt19937 m_Mt;
    static std::uniform_real_distribution<> m_UniformNormalizedReal;
};
