// From a sequence of actions over a replay, generate an array whici is
// an encoding of the researched tech over the game based on when the
// research action is requested and when it is expected to complete
// based on the game data
//  | -------R1Action----R1Fin ---R2Action------------R2Fin--------------
//  | -----------|---------|---------|------------------|----------------
//  R1 | 0000000000000000001111111111111111111111111111111111111111111111
//  R2 | 0000000000000000000000000000000000000000000000011111111111111111
//  ..
//  RN | 0000000000000000000000000000000000000000000000000000000000000000

#include "generated_info.hpp"
#include "replay_parsing.hpp"

#include <spdlog/spdlog.h>
#include <yaml-cpp/yaml.h>

#include <filesystem>
#include <limits>

namespace cvt {


UpgradeTiming::UpgradeTiming(std::string dataFile) : dataFile_(std::move(dataFile)) {}

void UpgradeTiming::setVersion(std::string_view version)
{
    if (version != gameVersion_) {
        gameVersion_ = version;
        this->loadInfo();
    }
}

// Based on game engine data, determine the times which research
// actions are completed and thus the upgrade is active.
void UpgradeTiming::loadInfo()
{
    if (!std::filesystem::exists(dataFile_)) {
        throw std::runtime_error(fmt::format("Data file does not exist: {}", dataFile_));
    }
    YAML::Node root = YAML::LoadFile(dataFile_);
    auto node = std::find_if(
        root.begin(), root.end(), [&](const YAML::Node &n) { return n["version"].as<std::string>() == gameVersion_; });
    if (node == root.end()) {
        throw std::runtime_error(fmt::format("Game Version {} not in data file {}", gameVersion_, dataFile_));
    }

    // Clear current mapping
    id2delay_.clear();

    const auto &upgrades = (*node)["upgrades"];
    for (auto &&upgrade : upgrades) {
        id2delay_[upgrade["ability_id"].as<int>()] = static_cast<int>(upgrade["time"].as<float>());
    }
}

void UpgradeTiming::setRace(Race race) noexcept { currentRace_ = race; }

auto UpgradeTiming::getValidIds() const -> const std::set<int> &
{
    switch (currentRace_) {
    case Race::Protoss:
        return protossResearch;
    case Race::Zerg:
        return zergResearch;
    case Race::Terran:
        return terranResearch;
    }
    throw std::out_of_range(fmt::format("Invalid enum type {}", static_cast<int>(currentRace_)));
}


// Returns a vector containing the time point when the index of an upgrade is researched
void UpgradeTiming::setActions(const std::vector<std::vector<Action>> &actionsReplay,
    const std::vector<unsigned int> &timeIdxs)
{
    if (id2delay_.empty()) { throw std::logic_error{ "Research info to delay not loaded" }; }
    if (currentRace_ == Race::Random) { throw std::logic_error{ "No race selected" }; }
    if (actionsReplay.size() != timeIdxs.size()) {
        throw std::runtime_error{ fmt::format(
            "Actions size {}: != timeIdx size: {}", actionsReplay.size(), timeIdxs.size()) };
    }

    const auto &raceUpgradeIds = this->getValidIds();
    upgradeTimes_.resize(raceUpgradeIds.size());
    std::ranges::fill(upgradeTimes_, std::numeric_limits<std::ranges::range_value_t<decltype(upgradeTimes_)>>::max());

    for (std::size_t idx = 0; idx < actionsReplay.size(); ++idx) {
        const auto &actionsStep = actionsReplay[idx];
        for (auto &&action : actionsStep) {
            const auto upgrade = raceUpgradeIds.find(action.ability_id);
            if (upgrade != raceUpgradeIds.end()) {
                std::size_t upgradeIdx = std::distance(raceUpgradeIds.begin(), upgrade);
                upgradeTimes_[upgradeIdx] = timeIdxs[idx] + id2delay_.at(*upgrade);
            }
        }
    }
}

}// namespace cvt
