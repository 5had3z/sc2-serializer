#include "converter.hpp"

#include <cstring>

namespace cvt {

auto Converter::loadDB(const std::filesystem::path &path) noexcept -> bool { return database_.open(path); }

void Converter::OnGameStart()
{
    // Set New Heightmap
    // currentReplay_.heightMap
}

void Converter::OnGameEnd()
{
    // Save entry to DB
    database_.addEntry(currentReplay_);
    currentReplay_.clear();
}

void Converter::OnStep()
{
    // Do Things
    if (currentReplay_.heightMap.empty()) { this->copyHeightMapData(); }
    this->copyUnitData();
    this->copyActionData();
    this->copyDynamicMapData();
}

void Converter::setReplayHash(const std::string_view hash) noexcept { currentReplay_.replayHash = hash; }

void Converter::copyHeightMapData() noexcept
{
    const auto *rawObs = this->Observation()->GetRawObservation();
    const auto hMapDesc = rawObs->feature_layer_data().minimap_renders().height_map();
    auto &hMap = currentReplay_.heightMap;
    hMap.resize(hMapDesc.size().y(), hMapDesc.size().x());
    std::memcpy(hMap.data(), hMapDesc.data().data(), hMap.size());
}

void Converter::copyUnitData() noexcept
{
    // Foo
    const auto unitData = this->Observation()->GetUnits();
}
void Converter::copyActionData() noexcept
{
    // Foo
    const auto actionData = this->Observation()->GetRawActions();
}

void Converter::copyDynamicMapData() noexcept
{
    // Foo
    const auto *rawObs = this->Observation()->GetRawObservation();
}

}// namespace cvt
