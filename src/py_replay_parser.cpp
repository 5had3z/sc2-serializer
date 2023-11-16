#include "replay_parsing.hpp"

namespace cvt {

ReplayParser::ReplayParser(const std::filesystem::path &dataFile) noexcept : upgrade_(dataFile) {}

void ReplayParser::parseReplay(ReplayDataSoA replayData)
{
    replayData_ = std::move(replayData);
    upgrade_.setVersion(replayData_.gameVersion);
    upgrade_.setActions(replayData_.actions, replayData_.gameStep);
}

auto ReplayParser::sample(std::size_t timeIdx) const noexcept -> py::dict
{
    py::dict result;
    result["upgrades"] = upgrade_.getState<float>(timeIdx);
    return result;
}

}// namespace cvt