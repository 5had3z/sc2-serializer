#pragma once

#include <boost/pfr.hpp>

#include <algorithm>
#include <array>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <ranges>
#include <span>
#include <string>
#include <vector>

#include "enums.hpp"

namespace cvt {

// ------ Basic Data Structs ------

typedef std::uint64_t UID;// Type that represents unique identifier in the game

template<typename T>
concept IsSoAType = requires(T x) {
    typename T::struct_type;
    {
        x[std::size_t{}]
    } -> std::same_as<typename T::struct_type>;
};

namespace detail {

    template<typename T, typename It>
        requires std::is_arithmetic_v<T>
    auto vectorize_helper(T d, It it, bool onehotEnum) -> It
    {
        *it++ = static_cast<It::container_type::value_type>(d);
        return it;
    }

    template<std::ranges::range T, typename It>
        requires std::is_arithmetic_v<std::ranges::range_value_t<T>>
    auto vectorize_helper(const T &d, It it, bool onehotEnum) -> It
    {
        return std::ranges::transform(d, it, [](auto e) { return static_cast<It::container_type::value_type>(e); }).out;
    }

    template<typename T, typename It>
        requires std::is_enum_v<T>
    auto vectorize_helper(T d, It it, bool onehotEnum) -> It
    {
        using value_type = It::container_type::value_type;
        if (onehotEnum) {
            it = std::ranges::copy(enumToOneHot<value_type>(d), it).out;
        } else {
            *it++ = static_cast<value_type>(d);
        }
        return it;
    }

    template<typename T, typename It>
        requires std::is_aggregate_v<T> && (!std::ranges::range<T>)
    auto vectorize_helper(T d, It it, bool onehotEnum) -> It
    {
        boost::pfr::for_each_field(
            d, [&it, onehotEnum](const auto &field) { it = detail::vectorize_helper(field, it, onehotEnum); });
        return it;
    }


}// namespace detail

template<typename S, typename It>
    requires std::is_aggregate_v<S> && std::is_arithmetic_v<typename It::container_type::value_type>
[[maybe_unused]] auto vectorize(S s, It it, bool onehotEnum = false) -> It
{
    boost::pfr::for_each_field(
        s, [&it, onehotEnum](const auto &field) { it = detail::vectorize_helper(field, it, onehotEnum); });
    return it;
}

// TODO: Add helper fn to check the vectorization size
template<typename T, typename S>
    requires std::is_aggregate_v<S> && std::is_arithmetic_v<T>
auto vectorize(S s, bool onehotEnum = false) -> std::vector<T>
{
    std::vector<T> out;
    vectorize(s, std::back_inserter(out), onehotEnum);
    return out;
}

template<typename AoS, typename SoA> [[nodiscard]] auto AoStoSoA(AoS &&aos) noexcept -> SoA;

template<typename AoS, typename SoA> [[nodiscard]] auto AoStoSoA(const AoS &aos) noexcept -> SoA;

template<typename SoA, typename AoS> [[nodiscard]] auto SoAtoAoS(const SoA &soa) noexcept -> AoS;

template<IsSoAType SoA> [[nodiscard]] auto SoAtoAoS(const SoA &soa) noexcept -> std::vector<typename SoA::struct_type>
{
    std::vector<typename SoA::struct_type> aos{};

    // Ensure SoA is all equally sized
    std::vector<std::size_t> sizes;
    boost::pfr::for_each_field(soa, [&](auto &field) { sizes.push_back(field.size()); });
    assert(std::all_of(sizes.begin(), sizes.end(), [sz = sizes.front()](std::size_t s) { return s == sz; }));
    aos.resize(sizes.front());

    // Copy data element-by-element
    for (std::size_t idx = 0; idx < sizes.front(); ++idx) { aos[idx] = soa[idx]; }
    return aos;
}

struct Point2d
{
    int x{ 0 };
    int y{ 0 };
    [[nodiscard]] auto operator==(const Point2d &other) const noexcept -> bool = default;

    [[nodiscard]] auto begin() noexcept -> int * { return &x; }
    [[nodiscard]] auto end() noexcept -> int * { return &y + 1; }

    [[nodiscard]] auto begin() const noexcept -> const int * { return &x; }
    [[nodiscard]] auto end() const noexcept -> const int * { return &y + 1; }
};

struct Point3f
{
    float x{ 0.f };
    float y{ 0.f };
    float z{ 0.f };
    [[nodiscard]] auto operator==(const Point3f &other) const noexcept -> bool = default;

    [[nodiscard]] auto begin() noexcept -> float * { return &x; }
    [[nodiscard]] auto end() noexcept -> float * { return &z + 1; }

    [[nodiscard]] auto begin() const noexcept -> const float * { return &x; }
    [[nodiscard]] auto end() const noexcept -> const float * { return &z + 1; }
};

/**
 * @brief
 * @tparam T
 */
template<typename T>
    requires std::is_arithmetic_v<T>
struct Image
{
    using value_type = T;
    using ptr_type = T *;
    using const_ptr_type = const T *;

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

    // Typed pointer to the data
    [[nodiscard]] auto data() noexcept -> ptr_type { return reinterpret_cast<ptr_type>(_data.data()); }

    // Const Typed pointer to the data
    [[nodiscard]] auto data() const noexcept -> const_ptr_type
    {
        return reinterpret_cast<const_ptr_type>(_data.data());
    }

    /**
     * @brief Typed modifiable view of the data, unavailable if value_type is bool
     */
    [[nodiscard]] auto as_span() noexcept -> std::span<value_type>
        requires(!std::same_as<value_type, bool>)
    {
        return std::span(this->data(), this->nelem());
    }

    /**
     * @brief Typed const view of the data, unavailable if value_type is bool
     */
    [[nodiscard]] auto as_span() const noexcept -> const std::span<const value_type>
        requires(!std::same_as<value_type, bool>)
    {
        return std::span(this->data(), this->nelem());
    }
};

struct Score
{
    float score_float;
    float idle_production_time;
    float idle_worker_time;
    float total_value_units;
    float total_value_structures;
    float killed_value_units;
    float killed_value_structures;
    float collected_minerals;
    float collected_vespene;
    float collection_rate_minerals;
    float collection_rate_vespene;
    float spent_minerals;
    float spent_vespene;

    float total_damage_dealt_life;
    float total_damage_dealt_shields;
    float total_damage_dealt_energy;

    float total_damage_taken_life;
    float total_damage_taken_shields;
    float total_damage_taken_energy;

    float total_healed_life;
    float total_healed_shields;
    float total_healed_energy;

    [[nodiscard]] auto operator==(const Score &other) const noexcept -> bool = default;
};


struct UnitOrder
{
    int ability_id{ 0 };
    //! Progress of the order.
    float progress{ 0.0 };
    //! Target unit of the order, if there is one.
    UID tgtId{ 0 };
    //! Target position of the order, if there is one.
    Point2d target_pos{ 0, 0 };

    [[nodiscard]] auto operator==(const UnitOrder &other) const noexcept -> bool = default;

    [[nodiscard]] operator std::string() const noexcept
    {
        return fmt::format("Order[ability: {}, prog: {}, tgtId: {}, tgtPos: ({},{})]",
            ability_id,
            progress,
            tgtId,
            target_pos.x,
            target_pos.y);
    }
};

struct Unit
{
    UID id{};
    UID tgtId{};
    Visibility observation{};
    Alliance alliance{};
    CloakState cloak_state{ CloakState::Unknown };
    AddOn add_on_tag{ AddOn::None };
    int unitType{};
    float health{};
    float health_max{};
    float shield{};
    float shield_max{};
    float energy{};
    float energy_max{};
    float weapon_cooldown{};
    int buff0{};
    int buff1{};
    Point3f pos{};// x
    // y
    // z
    float heading{};
    float radius{};
    float build_progress{};
    char cargo{};
    char cargo_max{};
    char assigned_harvesters{};
    char ideal_harvesters{};
    bool is_blip{ false };// detected by sensor
    bool is_flying{ false };// flying ship
    bool is_burrowed{ false };// zerg
    bool is_powered{ false };// pylon
    bool in_cargo{ false };
    UnitOrder order0{};
    UnitOrder order1{};
    UnitOrder order2{};
    UnitOrder order3{};

    [[nodiscard]] auto operator==(const Unit &other) const noexcept -> bool = default;

    [[nodiscard]] operator std::string() const noexcept
    {
        return fmt::format(
            "Unit[id: {}, tgtId: {}, obs: {}, alliance: {}, cloak: {}, add_on: {}, unitType: {}, health: {:.1f}, "
            "health_max: {:.1f}, shield: {:.1f}, shield_max: {:.1f}, energy: {:.1f}, energy_max: {:.1f}, "
            "weapon_cooldown: {:.1f}, buff0: {}, buff1: {}, pos: [{:.2f},{:.2f},{:.2f},{:.2f}], radius: {:.1f}, "
            "build_progress: {:.1f}, cargo: {}, cargo_max: {}, assigned_harv: {}, ideal_harv: {}, is_blip: {}, "
            "is_flying: {}, is_burrowed: {}, is_powered: {}, in_cargo: {}, order0: {}, order1: {}, order2: {}, order3: "
            "{}]",
            id,
            tgtId,
            observation,
            alliance,
            cloak_state,
            add_on_tag,
            unitType,
            health,
            health_max,
            shield,
            shield_max,
            energy,
            energy_max,
            weapon_cooldown,
            buff0,
            buff1,
            pos.x,
            pos.y,
            pos.z,
            heading,
            radius,
            build_progress,
            cargo,
            cargo_max,
            assigned_harvesters,
            ideal_harvesters,
            is_blip,
            is_flying,
            is_burrowed,
            is_powered,
            in_cargo,
            std::string(order0),
            std::string(order1),
            std::string(order2),
            std::string(order3));
    }

    friend std::ostream &operator<<(std::ostream &os, const Unit &unit) { return os << std::string(unit); }
};


struct UnitSoA
{
    using struct_type = Unit;
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
    std::vector<char> cargo{};
    std::vector<char> cargo_max{};
    std::vector<char> assigned_harvesters{};
    std::vector<char> ideal_harvesters{};
    std::vector<float> weapon_cooldown{};
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

    std::vector<AddOn> add_on_tag{};

    [[nodiscard]] auto operator==(const UnitSoA &other) const noexcept -> bool = default;

    [[nodiscard]] auto operator[](std::size_t idx) const noexcept -> Unit
    {
        Unit unit;
        unit.id = id[idx];
        unit.unitType = unitType[idx];
        unit.observation = observation[idx];
        unit.alliance = alliance[idx];
        unit.health = health[idx];
        unit.health_max = health_max[idx];
        unit.shield = shield[idx];
        unit.shield_max = shield_max[idx];
        unit.energy = energy[idx];
        unit.energy_max = energy_max[idx];
        unit.cargo = cargo[idx];
        unit.cargo_max = cargo_max[idx];
        unit.assigned_harvesters = assigned_harvesters[idx];
        unit.ideal_harvesters = ideal_harvesters[idx];
        unit.weapon_cooldown = weapon_cooldown[idx];
        unit.tgtId = tgtId[idx];
        unit.cloak_state = cloak_state[idx];
        unit.is_blip = is_blip[idx];
        unit.is_flying = is_flying[idx];
        unit.is_burrowed = is_burrowed[idx];
        unit.is_powered = is_powered[idx];
        unit.in_cargo = in_cargo[idx];
        unit.pos = pos[idx];
        unit.heading = heading[idx];
        unit.radius = radius[idx];
        unit.build_progress = build_progress[idx];
        unit.order0 = order0[idx];
        unit.order1 = order1[idx];
        unit.order2 = order2[idx];
        unit.order3 = order3[idx];
        unit.buff0 = buff0[idx];
        unit.buff1 = buff1[idx];
        unit.add_on_tag = add_on_tag[idx];
        return unit;
    }
};

template<std::ranges::range Range>
auto AoStoSoA(Range &&aos) noexcept -> UnitSoA
    requires std::is_same_v<std::ranges::range_value_t<Range>, Unit>
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


// Static Neutral Units such as VespeneGeysers are missing
// many of the common player unit properties and therefore
// should be handled separately to save space and better
// separate neutral entities and dynamic agents
struct NeutralUnit
{
    UID id{};
    int unitType{};
    float health{};
    float health_max{};
    Point3f pos{};// x
    // y
    // z
    float heading{};
    float radius{};
    std::uint16_t contents{};// minerals or vespene
    Visibility observation{};

    [[nodiscard]] auto operator==(const NeutralUnit &other) const noexcept -> bool = default;

    [[nodiscard]] operator std::string() const noexcept
    {
        return fmt::format(
            "NeutralUnit[id: {}, type: {}, health: {:.1f}, health_max: {:.1f}, pos: ({:.1f}, {:.1f}, {:.1f}, {:.1f}), "
            "radius: {:.1f}, contents: {}, vis: {}]",
            id,
            unitType,
            health,
            health_max,
            pos.x,
            pos.y,
            pos.z,
            heading,
            radius,
            contents,
            observation);
    }
};

struct NeutralUnitSoA
{
    using struct_type = NeutralUnit;
    std::vector<UID> id{};
    std::vector<int> unitType{};
    std::vector<Visibility> observation{};
    std::vector<float> health{};
    std::vector<float> health_max{};
    std::vector<Point3f> pos{};
    std::vector<float> heading{};
    std::vector<float> radius{};
    std::vector<std::uint16_t> contents{};

    [[nodiscard]] auto operator==(const NeutralUnitSoA &other) const noexcept -> bool = default;

    [[nodiscard]] auto operator[](std::size_t idx) const noexcept -> NeutralUnit
    {
        NeutralUnit unit;
        unit.id = id[idx];
        unit.unitType = unitType[idx];
        unit.observation = observation[idx];
        unit.health = health[idx];
        unit.health_max = health_max[idx];
        unit.pos = pos[idx];
        unit.heading = heading[idx];
        unit.radius = radius[idx];
        unit.contents = contents[idx];
        return unit;
    }
};

template<std::ranges::range Range>
auto AoStoSoA(Range &&aos) noexcept -> NeutralUnitSoA
    requires std::is_same_v<std::ranges::range_value_t<Range>, NeutralUnit>
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


struct Action
{
    enum TargetType { Self, OtherUnit, Position };

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
    TargetType target_type{ TargetType::Self };
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
        case Action::TargetType::Position:
            return target.point == other.target.point;
        case Action::TargetType::OtherUnit:
            return target.other == other.target.other;
        case Action::TargetType::Self:
            return true;
        }
        return false;// Unknown union type
    }
};

template<typename T> auto enumToOneHot(Action::TargetType e) noexcept -> std::vector<T>
{
    using E = Action::TargetType;
    constexpr std::array vals = { E::Self, E::OtherUnit, E::Position };
    return detail::enumToOneHot_helper<T>(e, vals);
}

struct StepData
{
    std::uint32_t gameStep{};
    std::uint16_t minearals{};
    std::uint16_t vespene{};
    std::uint16_t popMax{};
    std::uint16_t popArmy{};
    std::uint16_t popWorkers{};
    Score score{};
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

struct StepDataSoA
{
    using struct_type = StepData;
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

    [[nodiscard]] auto operator==(const StepDataSoA &other) const noexcept -> bool = default;

    [[nodiscard]] auto operator[](std::size_t idx) const noexcept -> StepData
    {
        StepData stepData;
        stepData.gameStep = gameStep[idx];
        stepData.minearals = minearals[idx];
        stepData.vespene = vespene[idx];
        stepData.popMax = popMax[idx];
        stepData.popArmy = popArmy[idx];
        stepData.popWorkers = popWorkers[idx];
        stepData.score = score[idx];
        stepData.visibility = visibility[idx];
        stepData.creep = creep[idx];
        stepData.player_relative = player_relative[idx];
        stepData.alerts = alerts[idx];
        stepData.buildable = buildable[idx];
        stepData.pathable = pathable[idx];
        stepData.actions = actions[idx];
        stepData.units = units[idx];
        stepData.neutralUnits = neutralUnits[idx];
        return stepData;
    }
};

static_assert(IsSoAType<StepDataSoA>);

template<std::ranges::range Range>
    requires std::same_as<std::ranges::range_value_t<Range>, StepData>
auto AoStoSoA(Range &&aos) noexcept -> StepDataSoA
{
    StepDataSoA soa;

    // Preallocate for expected size
    boost::pfr::for_each_field(soa, [&](auto &field) { field.reserve(aos.size()); });

    for (auto &&step : aos) {
        soa.gameStep.emplace_back(step.gameStep);
        soa.minearals.emplace_back(step.minearals);
        soa.vespene.emplace_back(step.vespene);
        soa.popMax.emplace_back(step.popMax);
        soa.popArmy.emplace_back(step.popArmy);
        soa.popWorkers.emplace_back(step.popWorkers);
        soa.score.emplace_back(step.score);
        soa.visibility.emplace_back(step.visibility);
        soa.creep.emplace_back(step.creep);
        soa.player_relative.emplace_back(step.player_relative);
        soa.alerts.emplace_back(step.alerts);
        soa.buildable.emplace_back(step.buildable);
        soa.pathable.emplace_back(step.pathable);
        soa.actions.emplace_back(step.actions);
        soa.units.emplace_back(step.units);
        soa.neutralUnits.emplace_back(step.neutralUnits);
    }
    return soa;
}

}// namespace cvt