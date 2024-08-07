/**
 * @file upgrade_state.cpp
 * @author Bryce Ferenczi
 * @brief From a sequence of actions over a replay, generate an array which is an encoding of the researched tech over
 * the game based on when the research action is requested and when it is expected to complete based on the game data.
 *
 * | -------R1Action----R1Fin ---R2Action------------R2Fin--------------
 * | -----------|---------|---------|------------------|----------------
 * R1 | 0000000000000000001111111111111111111111111111111111111111111111
 * R2 | 0000000000000000000000000000000000000000000000011111111111111111
 * ..
 * RN | 0000000000000000000000000000000000000000000000000000000000000000
 *
 * @version 0.1
 * @date 2024-05-28
 *
 * @copyright Copyright (c) 2024
 *
 */


#include "generated_info.hpp"
#include "replay_parsing.hpp"

#include <spdlog/spdlog.h>
#include <yaml-cpp/yaml.h>

#include <limits>

namespace cvt {

UpgradeState::UpgradeState(std::filesystem::path dataFile) : dataFile_(std::move(dataFile)) { this->loadInfo(); }

void UpgradeState::setVersion(std::string_view version)
{
    if (!gameVersion2id2delay_.contains(std::string(version))) {
        throw std::out_of_range(fmt::format("Game Version not found: {}", version));
    }
    gameVersion_ = version;
}

void UpgradeState::setRace(Race race) noexcept { currentRace_ = race; }

void UpgradeState::loadInfo()
{
    if (!std::filesystem::exists(dataFile_)) {
        throw std::runtime_error(fmt::format("Data file does not exist: {}", dataFile_.string()));
    }
    YAML::Node root = YAML::LoadFile(dataFile_.string());

    // Load all entries from yaml into memory
    for (auto &&entry : root) {
        std::unordered_map<int, int> id2delay = {};
        for (auto &&upgrade : entry["upgrades"]) {
            id2delay.emplace(upgrade["ability_id"].as<int>(), static_cast<int>(upgrade["time"].as<float>()));
        }
        gameVersion2id2delay_.emplace(entry["version"].as<std::string>(), std::move(id2delay));
    }
}

auto UpgradeState::getValidIds() const -> const std::set<int> &
{
    if (!raceResearch.contains(gameVersion_)) {
        throw std::out_of_range{ fmt::format("Missing game version {} from raceResearch", gameVersion_) };
    }
    return raceResearch.at(gameVersion_).at(currentRace_);
}

auto UpgradeState::getValidRemap() const -> const std::unordered_map<int, std::array<int, 3>> &
{
    if (!raceResearchReID.contains(gameVersion_)) {
        throw std::out_of_range{ fmt::format("Missing game version {} from raceResearchReID", gameVersion_) };
    }
    return raceResearchReID.at(gameVersion_).at(currentRace_);
}


void UpgradeState::calculateTimes(const std::vector<std::vector<Action>> &playerActions,
    const std::vector<unsigned int> &gameTime)
{
    auto &id2delay = gameVersion2id2delay_.at(gameVersion_);
    if (id2delay.empty()) { throw std::logic_error{ "Research info to delay not loaded" }; }
    if (currentRace_ == Race::Random) { throw std::logic_error{ "No race selected" }; }
    if (playerActions.size() != gameTime.size()) {
        throw std::runtime_error{ fmt::format(
            "Actions size {}: != timeIdx size: {}", playerActions.size(), gameTime.size()) };
    }

    const auto &raceUpgradeIds = this->getValidIds();
    const auto &raceUpgradeRemap = this->getValidRemap();
    upgradeTimes_.resize(raceUpgradeIds.size());
    constexpr auto maxTime = std::numeric_limits<std::ranges::range_value_t<decltype(upgradeTimes_)>>::max();
    std::ranges::fill(upgradeTimes_, maxTime);

    for (std::size_t idx = 0; idx < playerActions.size(); ++idx) {
        const auto &actionsStep = playerActions[idx];
        for (auto &&action : actionsStep) {
            // First check if the ability is in the normal upgrade actions
            const auto abilityPtr = raceUpgradeIds.find(action.ability_id);
            if (abilityPtr != raceUpgradeIds.end()) {
                const std::size_t upgradeIdx = std::distance(raceUpgradeIds.begin(), abilityPtr);
                if (!id2delay.contains(action.ability_id)) {
                    throw std::out_of_range{ fmt::format("Ability id {} not in id2delay table", action.ability_id) };
                }
                upgradeTimes_[upgradeIdx] = gameTime[idx] + id2delay.at(action.ability_id);
                continue;
            }

            // Then check if it is in the leveled upgrade actions
            const auto remapPtr = raceUpgradeRemap.find(action.ability_id);
            if (remapPtr != raceUpgradeRemap.end()) {
                // The first ability_id in the remapping that is maxTime is the lowest level unresearched
                for (auto &&remapAbilityId : remapPtr->second) {
                    const std::size_t upgradeIdx =
                        std::distance(raceUpgradeIds.begin(), raceUpgradeIds.find(remapAbilityId));
                    if (upgradeTimes_[upgradeIdx] == maxTime) {
                        if (!id2delay.contains(remapAbilityId)) {
                            throw std::out_of_range{ fmt::format(
                                "Ability id {} not in id2delay table", remapAbilityId) };
                        }
                        upgradeTimes_[upgradeIdx] = gameTime[idx] + id2delay.at(remapAbilityId);
                        break;
                    }
                }
            }
        }
    }
    // auto count = std::ranges::count_if(upgradeTimes_, [](int32_t e) { return e != maxTime; });
    // SPDLOG_INFO("Number of upgrades applied {}", count);
}

}// namespace cvt
