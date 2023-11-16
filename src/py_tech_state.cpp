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

#include "data.hpp"
#include "generated_info.hpp"

#include <spdlog/spdlog.h>
#include <yaml-cpp/yaml.h>

#include <filesystem>
#include <type_traits>
#include <unordered_map>
#include <vector>

namespace cvt {

// Based on game engine data, determine the times which research
// actions are completed and thus the upgrade is active.
class UpgradeTiming
{
  public:
    void loadDelayInfo(const std::string &dataFile, const std::string &version)
    {
        if (!std::filesystem::exists(dataFile)) {
            throw std::runtime_error(fmt::format("Data file does not exist: {}", dataFile));
        }
        YAML::Node root = YAML::LoadFile(dataFile);
        auto node = std::find_if(
            root.begin(), root.end(), [&](const YAML::Node &n) { return n["version"].as<std::string>() == version; });
        if (node == root.end()) {
            throw std::runtime_error(fmt::format("Game Version {} not in data file {}", version, dataFile));
        }

        // Clear current mapping
        id2delay_.clear();

        const auto &upgrades = (*node)["upgrades"];
        for (auto &&upgrade : upgrades) {
            id2delay_[upgrade["ability_id"].as<int>()] = static_cast<int>(upgrade["time"].as<float>());
        }
    }

    void setRace(Race race) noexcept { currentRace_ = race; }


    // Returns a vector containing the time point when the index of an upgrade is researched
    template<std::integral T = int32_t>
    [[nodiscard]] auto getResearchTimes(const std::vector<Action> &actions, const std::vector<int> &timeIdxs) const
        -> std::vector<T>
    {
        if (id2delay_.empty()) { throw std::logic_error{ "Research info to delay not loaded" }; }
        if (currentRace_ == Race::Random) { throw std::logic_error{ "No race selected" }; }
        if (actions.size() != timeIdxs.size()) {
            throw std::runtime_error{ fmt::format(
                "Actions size {}: != timeIdx size: {}", actions.size(), timeIdxs.size()) };
        }

        const auto &raceUpgradeIds = this->getValidIds();
        std::vector<T> upgradeTimes(raceUpgradeIds.size(), 0);

        for (std::size_t idx = 0; idx < actions.size(); ++idx) {
            const auto &action = actions[idx];
            const auto upgrade = raceUpgradeIds.find(action.ability_id);
            if (upgrade != raceUpgradeIds.end()) {
                std::size_t upgradeIdx = std::distance(raceUpgradeIds.begin(), upgrade);
                upgradeTimes[upgradeIdx] = timeIdxs[idx] + id2delay_.at(*upgrade);
            }
        }
        return upgradeTimes;
    }

  private:
    [[nodiscard]] auto getValidIds() const -> const std::set<int> &
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

    std::unordered_map<int, int> id2delay_{};
    Race currentRace_{ Race::Random };
};

}// namespace cvt
