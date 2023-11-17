#include "replay_parsing.hpp"

#include <pybind11/numpy.h>

#include <type_traits>

namespace cvt {

ReplayParser::ReplayParser(const std::filesystem::path &dataFile) noexcept : upgrade_(dataFile) {}

void ReplayParser::parseReplay(ReplayDataSoA replayData)
{
    replayData_ = std::move(replayData);
    upgrade_.setRace(replayData_.playerRace);
    upgrade_.setVersion(replayData_.gameVersion);
    upgrade_.setActions(replayData_.actions, replayData_.gameStep);
}

template<typename T>
    requires std::is_floating_point_v<T> || std::is_integral_v<T>
auto transformUnits(const std::vector<Unit> &units) noexcept -> py::array_t<T>
{
    if (units.empty()) { return py::array_t<T>(); }
}


auto ReplayParser::sample(std::size_t timeIdx) const noexcept -> py::dict
{
    py::dict result;
    result["upgrades"] = upgrade_.getState<float>(timeIdx);
    result["units"] = transformUnits<float>(replayData_.units[timeIdx]);
    return result;
}

}// namespace cvt
