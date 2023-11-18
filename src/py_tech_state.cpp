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

#include <limits>

namespace cvt {


UpgradeTiming::UpgradeTiming(std::filesystem::path dataFile) : dataFile_(std::move(dataFile)) {}

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
        throw std::runtime_error(fmt::format("Data file does not exist: {}", dataFile_.string()));
    }
    YAML::Node root = YAML::LoadFile(dataFile_);
    auto node = std::find_if(
        root.begin(), root.end(), [&](const YAML::Node &n) { return n["version"].as<std::string>() == gameVersion_; });
    if (node == root.end()) {
        throw std::runtime_error(fmt::format("Game Version {} not in data file {}", gameVersion_, dataFile_.string()));
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
    return raceResearch.at(gameVersion_).at(currentRace_);
}

auto UpgradeTiming::getValidRemap() const -> const std::unordered_map<int, std::array<int, 3>> &
{
    return raceResearchReID.at(gameVersion_).at(currentRace_);
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
    const auto &raceUpgradeRemap = this->getValidRemap();
    upgradeTimes_.resize(raceUpgradeIds.size());
    constexpr auto maxTime = std::numeric_limits<std::ranges::range_value_t<decltype(upgradeTimes_)>>::max();
    std::ranges::fill(upgradeTimes_, maxTime);

    for (std::size_t idx = 0; idx < actionsReplay.size(); ++idx) {
        const auto &actionsStep = actionsReplay[idx];
        for (auto &&action : actionsStep) {
            const auto abilityPtr = raceUpgradeIds.find(action.ability_id);
            if (abilityPtr != raceUpgradeIds.end()) {
                std::size_t upgradeIdx = std::distance(raceUpgradeIds.begin(), abilityPtr);
                upgradeTimes_[upgradeIdx] = timeIdxs[idx] + id2delay_.at(action.ability_id);
                continue;
            }
            const auto remapPtr = raceUpgradeRemap.find(action.ability_id);
            if (remapPtr != raceUpgradeRemap.end()) {
                SPDLOG_INFO("GOT NON-LEVELED UPGRADE: {}", action.ability_id);
                // The first ability_id in the remapping that is maxTime is the lowest level unresearched
                for (auto &&remapAbilityId : remapPtr->second) {
                    std::size_t upgradeIdx = std::distance(raceUpgradeIds.begin(), raceUpgradeIds.find(remapAbilityId));
                    if (upgradeTimes_[upgradeIdx] == maxTime) {
                        upgradeTimes_[upgradeIdx] = timeIdxs[idx] + id2delay_.at(remapAbilityId);
                        break;
                    }
                }
            }
        }
    }
    auto count = std::ranges::count_if(upgradeTimes_, [](int32_t e) { return e != maxTime; });
    SPDLOG_INFO("Number of upgrades applied {}", count);
}

}// namespace cvt
