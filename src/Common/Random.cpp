#include "Random.h"

std::random_device Random::m_Device;
std::mt19937 Random::m_Mt(Random::m_Device());
std::uniform_real_distribution<> Random::m_UniformNormalizedReal(0.0, 1.0);

f64 Random::F64()
{
    return m_UniformNormalizedReal(m_Mt);
}
