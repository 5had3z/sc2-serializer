#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace cvt {

// ------ Basic Data Structs ------

typedef std::uint64_t UID;// Type that represents unique identifier in the game

struct Point2d
{
    int x{ 0 };
    int y{ 0 };
    [[nodiscard]] auto operator==(const Point2d &other) const noexcept -> bool = default;
};

struct Point3f
{
    float x{ 0.f };
    float y{ 0.f };
    float z{ 0.f };
    [[nodiscard]] auto operator==(const Point3f &other) const noexcept -> bool = default;
};

template<typename T> struct Image
{
    using value_type = T;
    using ptr_type = T *;

    int _h = 0;
    int _w = 0;
    std::vector<std::byte> _data{};

    [[nodiscard]] auto operator==(const Image &other) const noexcept -> bool = default;

    // Number of elements in the image
    [[nodiscard]] auto nelem() const noexcept -> std::size_t { return _h * _w; }

    // Resize the buffer
    void resize(int height, int width)
    {
        _h = height;
        _w = width;
        if constexpr (std::is_same_v<bool, value_type>) {
            _data.resize(_h * _w / 8);
        } else {
            _data.resize(_h * _w * sizeof(value_type));
        }
    }

    // Clear buffer and shape
    void clear() noexcept
    {
        _data.clear();
        _h = 0;
        _w = 0;
    }

    // Size in bytes of the buffer
    [[nodiscard]] auto size() const noexcept -> std::size_t { return _data.size(); }

    // Uninitialized/empty buffer
    [[nodiscard]] auto empty() const noexcept -> bool { return _data.empty(); }

    // Raw pointer to the data with the correct type
    [[nodiscard]] auto data() noexcept -> ptr_type { return reinterpret_cast<ptr_type>(_data.data()); }
};

enum class Alliance {
    Self = 1,
    Ally = 2,
    Neutral = 3,
    Enemy = 4,
};

enum class CloakState {
    Unknown = 0,
    Cloaked = 1,
    Detected = 2,
    UnCloaked = 3,
    Allied = 4,
};

struct Unit
{
    UID id{};
    int unitType{};
    Alliance alliance{ Alliance::Self };
    float health{};
    float health_max{};
    float shield{};
    float shield_max{};
    float energy{};
    float energy_max{};
    int cargo{};
    int cargo_max{};
    UID tgtId{};
    CloakState cloak_state{ CloakState::Unknown };
    bool is_blip{ false };// detected by sensor
    bool is_flying{ false };// flying ship
    bool is_burrowed{ false };// zerg
    bool is_powered{ false };// pylon
    Point3f pos{};
    float heading{};
    float radius{};
    float build_progress{};

    [[nodiscard]] auto operator==(const Unit &other) const noexcept -> bool = default;
};

// template<typename... Ts> class aligned_union
// {
//   private:
//     alignas(Ts...) std::byte buff[std::max({ sizeof(Ts)... })];
// };
// using Target = aligned_union<Point2d, int>;

struct Action
{
    enum Target_Type { Self, OtherUnit, Position };

    // Use chat union and tag here because it's needed for serialization anyway
    union Target {
        Point2d point;
        UID other;
    };

    std::vector<UID> unit_ids{};
    int ability_id{};
    Target_Type target_type{ Target_Type::Self };
    Target target{};

    [[nodiscard]] auto operator==(const Action &other) const noexcept -> bool
    {
        bool ret = true;
        ret &= unit_ids == other.unit_ids;
        ret &= ability_id == other.ability_id;
        ret &= target_type == other.target_type;
        // Don't bother continue checking
        if (!ret) { return false; }

        // Check if the unions match depending on tag (skip self check)
        switch (target_type) {
        case Action::Target_Type::Position:
            return target.point == other.target.point;
        case Action::Target_Type::OtherUnit:
            return target.other == other.target.other;
        case Action::Target_Type::Self:
            return true;
        }
        return false;// Unknown union type
    }
};

struct StepData
{
    Image<std::uint8_t> visibility{};
    Image<bool> creep{};
    Image<std::uint8_t> player_relative{};
    Image<std::uint8_t> alerts{};
    Image<bool> buildable{};
    Image<bool> pathable{};

    std::vector<Action> actions{};
    std::vector<Unit> units{};

    [[nodiscard]] auto operator==(const StepData &other) const noexcept -> bool = default;
};

enum class Race { Terran, Zerg, Protoss, Random };

enum class Result { Win, Loss, Tie, Undecided };

struct ReplayData
{
    Image<std::uint8_t> heightMap{};
    std::vector<StepData> stepData{};
    std::string replayHash{};
    std::uint32_t playerId{};
    Race playerRace{ Race::Random };
    Result playerResult{ Result::Undecided };
    int playerMMR{};
    int playerAPM{};

    [[nodiscard]] auto operator==(const ReplayData &other) const noexcept -> bool = default;

    void clear() noexcept
    {
        heightMap.clear();
        stepData.clear();
        replayHash.clear();
    }
};

struct ReplayDataSoA
{
    Image<std::uint8_t> heightMap{};
    std::string replayHash{};
    std::uint32_t playerId{};
    Race playerRace{ Race::Random };
    Result playerResult{ Result::Undecided };
    int playerMMR{};
    int playerAPM{};

    // Step data
    std::vector<Image<std::uint8_t>> visibility{};
    std::vector<Image<bool>> creep{};
    std::vector<Image<std::uint8_t>> player_relative{};
    std::vector<Image<std::uint8_t>> alerts{};
    std::vector<Image<bool>> buildable{};
    std::vector<Image<bool>> pathable{};
    std::vector<std::vector<Action>> actions{};
    std::vector<std::vector<Unit>> units{};

    [[nodiscard]] auto operator==(const ReplayDataSoA &other) const noexcept -> bool = default;
};

[[nodiscard]] inline auto ReplayAoStoSoA(const ReplayData &aos) noexcept -> ReplayDataSoA
{
    ReplayDataSoA soa = { .heightMap = aos.heightMap,
        .replayHash = aos.replayHash,
        .playerId = aos.playerId,
        .playerRace = aos.playerRace,
        .playerResult = aos.playerResult,
        .playerMMR = aos.playerMMR,
        .playerAPM = aos.playerAPM };

    for (const StepData &step : aos.stepData) {
        soa.visibility.push_back(step.visibility);
        soa.creep.push_back(step.creep);
        soa.player_relative.push_back(step.player_relative);
        soa.alerts.push_back(step.alerts);
        soa.buildable.push_back(step.buildable);
        soa.pathable.push_back(step.pathable);
        soa.actions.push_back(step.actions);
        soa.units.push_back(step.units);
    }
    return soa;
}

[[nodiscard]] inline auto ReplaySoAtoAoS(const ReplayDataSoA &soa) noexcept -> ReplayData
{
    ReplayData aos = { .heightMap = soa.heightMap,
        .replayHash = soa.replayHash,
        .playerId = soa.playerId,
        .playerRace = soa.playerRace,
        .playerResult = soa.playerResult,
        .playerMMR = soa.playerMMR,
        .playerAPM = soa.playerAPM };

    auto &stepDataVec = aos.stepData;
    stepDataVec.resize(soa.units.size());

    for (std::size_t idx = 0; idx < stepDataVec.size(); ++idx) {
        auto &stepData = stepDataVec[idx];
        if (idx < soa.visibility.size()) { stepData.visibility = soa.visibility[idx]; }
        if (idx < soa.creep.size()) { stepData.creep = soa.creep[idx]; }
        if (idx < soa.player_relative.size()) { stepData.player_relative = soa.visibility[idx]; }
        if (idx < soa.alerts.size()) { stepData.alerts = soa.alerts[idx]; }
        if (idx < soa.buildable.size()) { stepData.buildable = soa.buildable[idx]; }
        if (idx < soa.pathable.size()) { stepData.pathable = soa.pathable[idx]; }
        if (idx < soa.actions.size()) { stepData.actions = soa.actions[idx]; }
        if (idx < soa.units.size()) { stepData.units = soa.units[idx]; }
    }
    return aos;
}

}// namespace cvt