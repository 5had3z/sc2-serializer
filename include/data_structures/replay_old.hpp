#pragma once

#include "../serialize.hpp"
#include "replay_all.hpp"
#include "replay_interface.hpp"

#include <ranges>

namespace cvt {

struct ReplayData
{
    std::string replayHash{};
    std::string gameVersion{};
    std::uint32_t playerId{};
    Race playerRace{ Race::Random };
    Result playerResult{ Result::Undecided };
    int playerMMR{};
    int playerAPM{};
    int mapWidth{};
    int mapHeight{};
    Image<std::uint8_t> heightMap{};
    std::vector<StepData> stepData{};

    [[nodiscard]] auto operator==(const ReplayData &other) const noexcept -> bool = default;
    [[nodiscard]] auto getReplayHash() noexcept -> std::string & { return replayHash; }
    [[nodiscard]] auto getReplayHash() const noexcept -> const std::string & { return replayHash; }
    [[nodiscard]] auto getPlayerId() noexcept -> std::uint32_t & { return playerId; }
    [[nodiscard]] auto getPlayerId() const noexcept -> std::uint32_t { return playerId; }
};

struct ReplayDataSoA
{
    using struct_type = ReplayData;
    std::string replayHash{};
    std::string gameVersion{};
    std::uint32_t playerId{};
    Race playerRace{ Race::Random };
    Result playerResult{ Result::Undecided };
    int playerMMR{};
    int playerAPM{};
    int mapWidth{};
    int mapHeight{};
    Image<std::uint8_t> heightMap{};

    // Step data
    std::vector<std::uint32_t> gameStep{};
    std::vector<std::uint16_t> minearals{};
    std::vector<std::uint16_t> vespene{};
    std::vector<std::uint16_t> popMax{};
    std::vector<std::uint16_t> popArmy{};
    std::vector<std::uint16_t> popWorkers{};
    std::vector<Score> score{};
    std::vector<Image<std::uint8_t>> visibility{};
    std::vector<Image<bool>> creep{};
    std::vector<Image<std::uint8_t>> player_relative{};
    std::vector<Image<std::uint8_t>> alerts{};
    std::vector<Image<bool>> buildable{};
    std::vector<Image<bool>> pathable{};
    std::vector<std::vector<Action>> actions{};
    std::vector<std::vector<Unit>> units{};
    std::vector<std::vector<NeutralUnit>> neutralUnits{};

    [[nodiscard]] auto operator==(const ReplayDataSoA &other) const noexcept -> bool = default;
    [[nodiscard]] auto getReplayHash() noexcept -> std::string & { return replayHash; }
    [[nodiscard]] auto getReplayHash() const noexcept -> const std::string & { return replayHash; }
    [[nodiscard]] auto getPlayerId() noexcept -> std::uint32_t & { return playerId; }
    [[nodiscard]] auto getPlayerId() const noexcept -> std::uint32_t { return playerId; }

    [[nodiscard]] auto operator[](std::size_t idx) const noexcept -> StepData
    {
        StepData stepData;
        if (idx < gameStep.size()) { stepData.gameStep = gameStep[idx]; }
        if (idx < minearals.size()) { stepData.minearals = minearals[idx]; }
        if (idx < vespene.size()) { stepData.vespene = vespene[idx]; }
        if (idx < popMax.size()) { stepData.popMax = popMax[idx]; }
        if (idx < popArmy.size()) { stepData.popArmy = popArmy[idx]; }
        if (idx < popWorkers.size()) { stepData.popWorkers = popWorkers[idx]; }
        if (idx < score.size()) { stepData.score = score[idx]; }
        if (idx < visibility.size()) { stepData.visibility = visibility[idx]; }
        if (idx < creep.size()) { stepData.creep = creep[idx]; }
        if (idx < player_relative.size()) { stepData.player_relative = player_relative[idx]; }
        if (idx < alerts.size()) { stepData.alerts = alerts[idx]; }
        if (idx < buildable.size()) { stepData.buildable = buildable[idx]; }
        if (idx < pathable.size()) { stepData.pathable = pathable[idx]; }
        if (idx < actions.size()) { stepData.actions = actions[idx]; }
        if (idx < units.size()) { stepData.units = units[idx]; }
        if (idx < neutralUnits.size()) { stepData.neutralUnits = neutralUnits[idx]; }
        return stepData;
    }
};

template<> struct DatabaseInterface<ReplayDataSoA>
{
    static auto getHashIdImpl(std::istream &dbStream) -> std::pair<std::string, std::uint32_t>
    {
        std::string replayHash{};
        std::string gameVersion{};
        std::uint32_t playerId{};
        deserialize(replayHash, dbStream);
        deserialize(gameVersion, dbStream);
        deserialize(playerId, dbStream);
        return std::make_pair(replayHash, playerId);
    }


    static auto getHeaderImpl(std::istream &dbStream) -> ReplayInfo
    {
        ReplayInfo result;
        deserialize(result.replayHash, dbStream);
        deserialize(result.gameVersion, dbStream);
        deserialize(result.playerId, dbStream);
        deserialize(result.playerRace, dbStream);
        deserialize(result.playerResult, dbStream);
        deserialize(result.playerMMR, dbStream);
        deserialize(result.playerAPM, dbStream);
        deserialize(result.mapWidth, dbStream);
        deserialize(result.mapHeight, dbStream);
        deserialize(result.heightMap, dbStream);

        // Won't be entirely accurate, last recorded observation != duration
        std::vector<std::uint32_t> gameSteps;
        deserialize(gameSteps, dbStream);
        result.durationSteps = gameSteps.back();
        return result;
    }

    static auto getEntryImpl(std::istream &dbStream) -> ReplayDataSoA
    {
        ReplayDataSoA result;
        deserialize(result, dbStream);
        return result;
    }

    static auto addEntryImpl(const ReplayDataSoA &d, std::ostream &dbStream) noexcept -> bool
    {
        serialize(d, dbStream);
        return true;
    }
};


}// namespace cvt
