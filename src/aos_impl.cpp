/**
 * @file array_struct_impl.cpp
 * @author Bryce Ferenczi
 * @brief Special implementations for AoS<->SoA where a sub-component of the struct is the actual AoS part.
 * @version 0.1
 * @date 2024-05-28
 *
 * @copyright Copyright (c) 2024
 *
 */

#include "data_structures/replay_all.hpp"

namespace cvt {

template<> auto AoStoSoA(const ReplayData &aos) noexcept -> ReplayDataSoA
{
    ReplayDataSoA soa;
    soa.header = aos.header;
    soa.data = AoStoSoA<StepDataSoA>(aos.data);
    return soa;
}

template<> auto SoAtoAoS(const ReplayDataSoA &soa) noexcept -> ReplayData
{
    ReplayData aos;
    aos.header = soa.header;
    aos.data = SoAtoAoS(soa.data);
    return aos;
}

}// namespace cvt
