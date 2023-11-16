#pragma once

#include <boost/pfr.hpp>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <ranges>
#include <string>
#include <vector>

namespace cvt {

// ------ Basic Data Structs ------

typedef std::uint64_t UID;// Type that represents unique identifier in the game


// Converts an enum value to a one-hot encoding
template<typename E, typename T>
    requires std::is_enum_v<E>
auto enumToOneHot(E e) noexcept -> std::vector<T>;

namespace detail {

    template<typename T, typename It>
        requires std::is_arithmetic_v<T>
    void vectorize_helper(T d, It &it, bool onehotEnum)
    {
        *it++ = d;
    }

    template<std::ranges::range T, typename It>
        requires std::is_arithmetic_v<std::ranges::range_value_t<T>>
    void vectorize_helper(const T &d, It &it, bool onehotEnum)
    {
        it = std::copy(d.cbegin(), d.cend(), it);
    }

    template<typename T, typename It>
        requires std::is_enum_v<T>
    void vectorize_helper(T d, It &it, bool onehotEnum)
    {
        using value_type = It::container_type::value_type;
        if (onehotEnum) {
            const auto onehot = enumToOneHot<value_type>(d);
            it = std::copy(onehot.cbegin(), onehot.cend(), it);
        } else {
            *it++ = static_cast<value_type>(d);
        }
    }

    template<typename T, typename It>
        requires std::is_aggregate_v<T> && (!std::ranges::range<T>)
    void vectorize_helper(T d, It &it, bool onehotEnum)
    {
        boost::pfr::for_each_field(
            d, [&it, onehotEnum](const auto &field) { detail::vectorize_helper(field, it, onehotEnum); });
    }


    template<typename T>
    auto enumToOneHot_helper(auto enumVal, const std::ranges::range auto &enumValues) -> std::vector<T>
    {
        auto it = std::ranges::find(enumValues, enumVal);
        std::vector<T> ret(enumValues.size());
        ret[std::distance(enumValues.begin(), it)] = static_cast<T>(1);
        return ret;
    }

}// namespace detail

template<typename T, typename S>
    requires std::is_aggregate_v<S>
auto vectorize(S s, bool onehotEnum = false) -> std::vector<T>
{
    std::vector<T> out;
    auto it = std::back_inserter(out);
    boost::pfr::for_each_field(
        s, [&it, onehotEnum](const auto &field) { detail::vectorize_helper(field, it, onehotEnum); });
    return out;
}


struct Point2d
{
    int x{ 0 };
    int y{ 0 };
    [[nodiscard]] auto operator==(const Point2d &other) const noexcept -> bool = default;

    [[nodiscard]] auto begin() noexcept -> int * { return &x; }
    [[nodiscard]] auto end() noexcept -> int * { return &y + 1; }

    [[nodiscard]] auto cbegin() const noexcept -> const int * { return &x; }
    [[nodiscard]] auto cend() const noexcept -> const int * { return &y + 1; }
};

struct Point3f
{
    float x{ 0.f };
    float y{ 0.f };
    float z{ 0.f };
    [[nodiscard]] auto operator==(const Point3f &other) const noexcept -> bool = default;

    [[nodiscard]] auto begin() noexcept -> float * { return &x; }
    [[nodiscard]] auto end() noexcept -> float * { return &z + 1; }

    [[nodiscard]] auto cbegin() const noexcept -> const float * { return &x; }
    [[nodiscard]] auto cend() const noexcept -> const float * { return &z + 1; }
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
            assert(height * width % 8 == 0 && "For bool type, height * width must be divisible by 8.");
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

enum class Alliance { Self = 1, Ally = 2, Neutral = 3, Enemy = 4 };

template<typename T> auto enumToOneHot(Alliance e) noexcept -> std::vector<T>
{
    constexpr std::array vals = std::array{ Alliance::Self, Alliance::Ally, Alliance::Neutral, Alliance::Enemy };
    static_assert(std::is_sorted(vals.begin(), vals.end()));
    return detail::enumToOneHot_helper<T>(e, vals);
}

enum class CloakState { Unknown = 0, Cloaked = 1, Detected = 2, UnCloaked = 3, Allied = 4 };

template<typename T> auto enumToOneHot(CloakState e) noexcept -> std::vector<T>
{
    constexpr std::array vals = {
        CloakState::Unknown, CloakState::Cloaked, CloakState::Detected, CloakState::UnCloaked, CloakState::Allied
    };
    static_assert(std::is_sorted(vals.begin(), vals.end()));
    return detail::enumToOneHot_helper<T>(e, vals);
}

enum class Visibility { Visible = 1, Snapshot = 2, Hidden = 3 };

template<typename T> auto enumToOneHot(Visibility e) noexcept -> std::vector<T>
{
    constexpr std::array vals = { Visibility::Visible, Visibility::Snapshot, Visibility::Hidden };
    static_assert(std::is_sorted(vals.begin(), vals.end()));
    return detail::enumToOneHot_helper<T>(e, vals);
}

enum class AddOn { None = 0, Reactor = 1, TechLab = 2 };

template<typename T> auto enumToOneHot(AddOn e) noexcept -> std::vector<T>
{
    constexpr std::array vals = { AddOn::None, AddOn::Reactor, AddOn::TechLab };
    static_assert(std::is_sorted(vals.begin(), vals.end()));
    return detail::enumToOneHot_helper<T>(e, vals);
}


struct UnitOrder
{
    // AbilityID ability_id;
    //! Target unit of the order, if there is one.
    UID tgtId{ 0 };
    //! Target position of the order, if there is one.
    Point2d target_pos{ 0, 0 };
    //! Progress of the order.
    float progress{ 0.0 };

    [[nodiscard]] auto operator==(const UnitOrder &other) const noexcept -> bool = default;
};

struct Unit
{
    UID id{};
    UID tgtId{};
    Visibility observation{};
    Alliance alliance{};
    CloakState cloak_state{ CloakState::Unknown };
    int unitType{};
    float health{};
    float health_max{};
    float shield{};
    float shield_max{};
    float energy{};
    float energy_max{};
    int cargo{};
    int cargo_max{};
    int assigned_harvesters{};
    int ideal_harvesters{};
    float weapon_cooldown{};
    Point3f pos{};// x
    // y
    // z
    float heading{};
    float radius{};
    float build_progress{};
    bool is_blip{ false };// detected by sensor
    bool is_flying{ false };// flying ship
    bool is_burrowed{ false };// zerg
    bool is_powered{ false };// pylon
    bool in_cargo{ false };

    UnitOrder order0;
    UnitOrder order1;
    UnitOrder order2;
    UnitOrder order3;
    int buff0;
    int buff1;
    AddOn add_on_tag{ AddOn::None };


    [[nodiscard]] auto operator==(const Unit &other) const noexcept -> bool = default;
};


struct UnitSoA
{
    std::vector<UID> id{};
    std::vector<int> unitType{};
    std::vector<Visibility> observation{};
    std::vector<Alliance> alliance{};
    std::vector<float> health{};
    std::vector<float> health_max{};
    std::vector<float> shield{};
    std::vector<float> shield_max{};
    std::vector<float> energy{};
    std::vector<float> energy_max{};
    std::vector<int> cargo{};
    std::vector<int> cargo_max{};
    std::vector<int> assigned_harvesters{};
    std::vector<int> ideal_harvesters{};
    std::vector<int> weapon_cooldown{};
    std::vector<UID> tgtId{};
    std::vector<CloakState> cloak_state{};

    // Vector Bool is a pain to deal with, this should be reasonably compressible anyway
    std::vector<char> is_blip{};// detected by sensor
    std::vector<char> is_flying{};// flying ship
    std::vector<char> is_burrowed{};// zerg
    std::vector<char> is_powered{};// pylon
    std::vector<char> in_cargo{};// pylon

    std::vector<Point3f> pos{};
    std::vector<UnitOrder> order0{};
    std::vector<UnitOrder> order1{};
    std::vector<UnitOrder> order2{};
    std::vector<UnitOrder> order3{};

    std::vector<int> buff0{};
    std::vector<int> buff1{};

    std::vector<float> heading{};
    std::vector<float> radius{};
    std::vector<float> build_progress{};

    std::vector<AddOn> add_on_tag{};// todo


    [[nodiscard]] auto operator==(const UnitSoA &other) const noexcept -> bool = default;
};


[[nodiscard]] inline auto UnitAoStoSoA(const std::vector<Unit> &aos) noexcept -> UnitSoA
{
    UnitSoA soa{};
    // Prealloc expected size
    boost::pfr::for_each_field(soa, [sz = aos.size()](auto &field) { field.reserve(sz); });

    // Tediously copy data
    for (const Unit &unit : aos) {
        soa.id.push_back(unit.id);
        soa.unitType.push_back(unit.unitType);
        soa.observation.push_back(unit.observation);
        soa.alliance.push_back(unit.alliance);
        soa.health.push_back(unit.health);
        soa.health_max.push_back(unit.health_max);
        soa.shield.push_back(unit.shield);
        soa.shield_max.push_back(unit.shield_max);
        soa.energy.push_back(unit.energy);
        soa.energy_max.push_back(unit.energy_max);
        soa.cargo.push_back(unit.cargo);
        soa.cargo_max.push_back(unit.cargo_max);
        soa.assigned_harvesters.push_back(unit.assigned_harvesters);
        soa.ideal_harvesters.push_back(unit.ideal_harvesters);
        soa.weapon_cooldown.push_back(unit.weapon_cooldown);

        soa.tgtId.push_back(unit.tgtId);
        soa.cloak_state.push_back(unit.cloak_state);
        soa.is_blip.push_back(unit.is_blip);
        soa.is_flying.push_back(unit.is_flying);
        soa.is_burrowed.push_back(unit.is_burrowed);
        soa.is_powered.push_back(unit.is_powered);
        soa.in_cargo.push_back(unit.in_cargo);
        soa.pos.push_back(unit.pos);
        soa.heading.push_back(unit.heading);
        soa.radius.push_back(unit.radius);
        soa.build_progress.push_back(unit.build_progress);

        soa.order0.push_back(unit.order0);
        soa.order1.push_back(unit.order1);
        soa.order2.push_back(unit.order2);
        soa.order3.push_back(unit.order3);
        soa.buff0.push_back(unit.buff0);
        soa.buff1.push_back(unit.buff1);
        soa.add_on_tag.push_back(unit.add_on_tag);
    }
    return soa;
}

[[nodiscard]] inline auto UnitSoAtoAoS(const UnitSoA &soa) noexcept -> std::vector<Unit>
{
    std::vector<Unit> aos{};

    // Ensure SoA is all equally sized
    std::vector<std::size_t> sizes;
    boost::pfr::for_each_field(soa, [&](auto &field) { sizes.push_back(field.size()); });
    assert(std::all_of(sizes.begin(), sizes.end(), [sz = sizes.front()](std::size_t s) { return s == sz; }));
    aos.resize(sizes.front());

    // Tediously copy data
    for (std::size_t idx = 0; idx < sizes.front(); ++idx) {
        auto &unit = aos[idx];
        unit.id = soa.id[idx];
        unit.unitType = soa.unitType[idx];
        unit.observation = soa.observation[idx];
        unit.alliance = soa.alliance[idx];
        unit.health = soa.health[idx];
        unit.health_max = soa.health_max[idx];
        unit.shield = soa.shield[idx];
        unit.shield_max = soa.shield_max[idx];
        unit.energy = soa.energy[idx];
        unit.energy_max = soa.energy_max[idx];
        unit.cargo = soa.cargo[idx];
        unit.cargo_max = soa.cargo_max[idx];
        unit.assigned_harvesters = soa.assigned_harvesters[idx];
        unit.ideal_harvesters = soa.ideal_harvesters[idx];
        unit.weapon_cooldown = soa.weapon_cooldown[idx];
        unit.tgtId = soa.tgtId[idx];
        unit.cloak_state = soa.cloak_state[idx];
        unit.is_blip = soa.is_blip[idx];
        unit.is_flying = soa.is_flying[idx];
        unit.is_burrowed = soa.is_burrowed[idx];
        unit.is_powered = soa.is_powered[idx];
        unit.in_cargo = soa.in_cargo[idx];
        unit.pos = soa.pos[idx];
        unit.heading = soa.heading[idx];
        unit.radius = soa.radius[idx];
        unit.build_progress = soa.build_progress[idx];
        unit.order0 = soa.order0[idx];
        unit.order1 = soa.order1[idx];
        unit.order2 = soa.order2[idx];
        unit.order3 = soa.order3[idx];
        unit.buff0 = soa.buff0[idx];
        unit.buff1 = soa.buff1[idx];
        unit.add_on_tag = soa.add_on_tag[idx];
    }
    return aos;
}

// Static Neutral Units such as VespeneGeysers are missing
// many of the common player unit properties and therefore
// should be handled separately to save space and better
// separate neutral entities and dynamic agents
struct NeutralUnit
{
    UID id{};
    int unitType{};
    Visibility observation{};
    float health{};
    float health_max{};
    Point3f pos{};// x
    // y
    // z
    float heading{};
    float radius{};
    int contents{};// minerals or vespene

    [[nodiscard]] auto operator==(const NeutralUnit &other) const noexcept -> bool = default;
};

struct NeutralUnitSoA
{
    std::vector<UID> id{};
    std::vector<int> unitType{};
    std::vector<Visibility> observation{};
    std::vector<float> health{};
    std::vector<float> health_max{};
    std::vector<Point3f> pos{};
    std::vector<float> heading{};
    std::vector<float> radius{};
    std::vector<int> contents{};

    [[nodiscard]] auto operator==(const NeutralUnitSoA &other) const noexcept -> bool = default;
};

[[nodiscard]] inline auto NeutralUnitAoStoSoA(const std::vector<NeutralUnit> aos) noexcept -> NeutralUnitSoA
{
    NeutralUnitSoA soa{};
    // Prealloc expected size
    boost::pfr::for_each_field(soa, [sz = aos.size()](auto &field) { field.reserve(sz); });

    for (auto &&unit : aos) {
        soa.id.push_back(unit.id);
        soa.unitType.push_back(unit.unitType);
        soa.observation.push_back(unit.observation);
        soa.health.push_back(unit.health);
        soa.health_max.push_back(unit.health_max);
        soa.pos.push_back(unit.pos);
        soa.heading.push_back(unit.heading);
        soa.radius.push_back(unit.radius);
        soa.contents.push_back(unit.contents);
    }
    return soa;
}

[[nodiscard]] inline auto NeutralUnitSoAtoAoS(const NeutralUnitSoA &soa) noexcept -> std::vector<NeutralUnit>
{
    std::vector<NeutralUnit> aos{};

    // Ensure SoA is all equally sized
    std::vector<std::size_t> sizes;
    boost::pfr::for_each_field(soa, [&](auto &field) { sizes.push_back(field.size()); });
    assert(std::all_of(sizes.begin(), sizes.end(), [sz = sizes.front()](std::size_t s) { return s == sz; }));
    aos.resize(sizes.front());

    // Tediously copy data
    for (std::size_t idx = 0; idx < sizes.front(); ++idx) {
        auto &unit = aos[idx];
        unit.id = soa.id[idx];
        unit.unitType = soa.unitType[idx];
        unit.observation = soa.observation[idx];
        unit.health = soa.health[idx];
        unit.health_max = soa.health_max[idx];
        unit.pos = soa.pos[idx];
        unit.heading = soa.heading[idx];
        unit.radius = soa.radius[idx];
        unit.contents = soa.contents[idx];
    }
    return aos;
}


struct Action
{
    enum Target_Type { Self, OtherUnit, Position };

    // Use chat union and tag here because it's needed for serialization anyway
    union Target {
        Point2d point;
        UID other;
        // Provide a default constructor to avoid Pybind11's error
        Target() { memset(this, 0, sizeof(Target)); }
        explicit Target(Point2d &&d) noexcept : point(d) {}
        explicit Target(const Point2d &d) noexcept : point(d) {}
        auto operator=(Point2d &&d) noexcept -> Target &
        {
            point = d;
            return *this;
        }

        explicit Target(UID &&d) noexcept : other(d) {}
        explicit Target(const UID &d) noexcept : other(d) {}
        auto operator=(UID &&d) noexcept -> Target &
        {
            other = d;
            return *this;
        }
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

template<typename T> auto enumToOneHot(Action::Target_Type e) noexcept -> std::vector<T>
{
    using E = Action::Target_Type;
    constexpr std::array vals = { E::Self, E::OtherUnit, E::Position };
    return detail::enumToOneHot_helper<T>(e, vals);
}

struct StepData
{
    std::uint32_t gameStep{};
    std::uint32_t minearals{};
    std::uint32_t vespere{};
    std::uint32_t popMax{};
    std::uint32_t popArmy{};
    std::uint32_t popWorkers{};
    Image<std::uint8_t> visibility{};
    Image<bool> creep{};
    Image<std::uint8_t> player_relative{};
    Image<std::uint8_t> alerts{};
    Image<bool> buildable{};
    Image<bool> pathable{};

    std::vector<Action> actions{};
    std::vector<Unit> units{};
    std::vector<NeutralUnit> neutralUnits{};

    [[nodiscard]] auto operator==(const StepData &other) const noexcept -> bool = default;
};

enum class Race { Terran, Zerg, Protoss, Random };

template<typename T> auto enumToOneHot(Race e) noexcept -> std::vector<T>
{
    constexpr std::array vals = { Race::Terran, Race::Zerg, Race::Protoss, Race::Random };
    static_assert(std::is_sorted(vals.begin(), vals.end()));
    return detail::enumToOneHot_helper<T>(e, vals);
}

enum class Result { Win, Loss, Tie, Undecided };

template<typename T> auto enumToOneHot(Result e) noexcept -> std::vector<T>
{
    constexpr std::array vals = { Result::Win, Result::Loss, Result::Tie, Result::Undecided };
    static_assert(std::is_sorted(vals.begin(), vals.end()));
    return detail::enumToOneHot_helper<T>(e, vals);
}

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

    void clear() noexcept
    {
        heightMap.clear();
        stepData.clear();
        replayHash.clear();
    }
};

struct ReplayDataSoA
{
    std::string replayHash{};
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
};

[[nodiscard]] inline auto ReplayAoStoSoA(const ReplayData &aos) noexcept -> ReplayDataSoA
{
    ReplayDataSoA soa = {
        .replayHash = aos.replayHash,
        .playerId = aos.playerId,
        .playerRace = aos.playerRace,
        .playerResult = aos.playerResult,
        .playerMMR = aos.playerMMR,
        .playerAPM = aos.playerAPM,
        .heightMap = aos.heightMap,
    };

    for (const StepData &step : aos.stepData) {
        soa.gameStep.push_back(step.gameStep);
        soa.visibility.push_back(step.visibility);
        soa.creep.push_back(step.creep);
        soa.player_relative.push_back(step.player_relative);
        soa.alerts.push_back(step.alerts);
        soa.buildable.push_back(step.buildable);
        soa.pathable.push_back(step.pathable);
        soa.actions.push_back(step.actions);
        soa.units.push_back(step.units);
        soa.neutralUnits.push_back(step.neutralUnits);
    }
    return soa;
}

[[nodiscard]] inline auto ReplaySoAtoAoS(const ReplayDataSoA &soa) noexcept -> ReplayData
{
    ReplayData aos = {
        .replayHash = soa.replayHash,
        .playerId = soa.playerId,
        .playerRace = soa.playerRace,
        .playerResult = soa.playerResult,
        .playerMMR = soa.playerMMR,
        .playerAPM = soa.playerAPM,
        .heightMap = soa.heightMap,
    };

    auto &stepDataVec = aos.stepData;
    stepDataVec.resize(soa.units.size());

    for (std::size_t idx = 0; idx < stepDataVec.size(); ++idx) {
        auto &stepData = stepDataVec[idx];
        if (idx < soa.gameStep.size()) { stepData.gameStep = soa.gameStep[idx]; }
        if (idx < soa.visibility.size()) { stepData.visibility = soa.visibility[idx]; }
        if (idx < soa.creep.size()) { stepData.creep = soa.creep[idx]; }
        if (idx < soa.player_relative.size()) { stepData.player_relative = soa.player_relative[idx]; }
        if (idx < soa.alerts.size()) { stepData.alerts = soa.alerts[idx]; }
        if (idx < soa.buildable.size()) { stepData.buildable = soa.buildable[idx]; }
        if (idx < soa.pathable.size()) { stepData.pathable = soa.pathable[idx]; }
        if (idx < soa.actions.size()) { stepData.actions = soa.actions[idx]; }
        if (idx < soa.units.size()) { stepData.units = soa.units[idx]; }
        if (idx < soa.neutralUnits.size()) { stepData.neutralUnits = soa.neutralUnits[idx]; }
    }
    return aos;
}

}// namespace cvt
