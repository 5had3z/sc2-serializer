/**
 * @brief Specialisation implementations for AoS<->SoA for structures to prevent redefinition errors if it were in the
 * header files.
 */

#include "data_structures/replay_all.hpp"

namespace cvt {

template<> auto AoStoSoA(const ReplayData &aos) noexcept -> ReplayDataSoA
{
    ReplayDataSoA soa;
    soa.header = aos.header;
    soa.data = AoStoSoA(aos.data);
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
