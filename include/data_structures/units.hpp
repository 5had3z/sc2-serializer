/**
 * @file units.hpp
 * @author Bryce Ferenczi
 * @brief Unit data structures for StarCraft II. NeutralUnits are treated specially as many properties such as buffs and
 * cloak_state are never utilized, so we save precious bytes and make a smaller structure especially for neutral
 * structures. This file also contains the instance-major transformation code for unit data which improves data
 * compression and reduces file size from unit data by ~70%.
 * @version 0.1
 * @date 2024-05-27
 *
 * @copyright Copyright (c) 2024
 *
 */
#pragma once

#include "common.hpp"
#include "enums.hpp"

#include <ranges>

namespace cvt {

/**
 * @brief Element in order queue for unit to follow.
 */
struct UnitOrder
{
    /**
     * @brief ID of order to do
     */
    int ability_id{ 0 };

    /**
     * @brief Progress of the order.
     */
    float progress{ 0.0 };

    /**
     * @brief Target unit of the order, if there is one.
     */
    UID tgtId{ 0 };

    /**
     * @brief Target position of the order, if there is one.
     */
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

/**
 * @brief Basic StarCraft II Unit Data
 */
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


/**
 * @brief SoA representation of a collection of StarCraft II units.
 */
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

    /**
     * @brief Gather unit data at index
     * @param idx index in SoA to gather data from
     * @return Unit
     */
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

/**
 * @brief Convert range of units from AoS form to SoA
 *
 * @tparam Range type of range of collection of units
 * @param aos data to transform to SoA
 * @return UnitSoA representation of input
 */
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


/**
 * @brief Static Neutral units in the Game
 *
 * @note Static Neutral Units such as VespeneGeysers are missing many of the common player unit properties and therefore
 * are handled separately to save space and better separate neutral entities and dynamic agents.
 */
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

/**
 * @brief SoA representation of a collection of Neutral Units
 */
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

/**
 * @brief Convert range of neutral units from AoS form to SoA
 *
 * @tparam Range type of range of collection of neutral units
 * @param aos data to transform to SoA
 * @return NeutralUnitSoA representation of input
 */
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

// -----------------------------------------------------------------
// FLATTENING VERSION 1
// -----------------------------------------------------------------

/**
 * @brief Flattened units in SoA form with associated step index
 * @tparam UnitSoAT Type of flattened unit
 */
template<typename UnitSoAT> struct FlattenedUnits
{
    UnitSoAT units;
    std::vector<std::uint32_t> indices;
};

/**
 * @brief First version of instance-major unit sorting, includes index with each unit as a separate vector.
 *
 * @tparam UnitSoAT unit structure type
 * @param replayUnits unit data to rearrange
 * @return FlattenedUnits<UnitSoAT> instance-major units
 */
template<IsSoAType UnitSoAT>
[[nodiscard]] auto flattenAndSortUnits(
    const std::vector<std::vector<typename UnitSoAT::struct_type>> &replayUnits) noexcept -> FlattenedUnits<UnitSoAT>
{
    using UnitT = UnitSoAT::struct_type;
    using UnitStepT = std::pair<std::uint32_t, UnitT>;
    std::vector<UnitStepT> unitStepFlatten;
    for (std::size_t idx = 0; idx < replayUnits.size(); ++idx) {
        std::ranges::transform(
            replayUnits[idx], std::back_inserter(unitStepFlatten), [=](UnitT u) { return std::make_pair(idx, u); });
    }

    // Significantly better compressibility when sorted by unit (and implicitly time)
    std::ranges::stable_sort(
        unitStepFlatten, [](const UnitStepT &a, const UnitStepT &b) { return a.second.id < b.second.id; });

    // Create flattened SoA
    UnitSoAT unitsSoA = cvt::AoStoSoA(std::views::values(unitStepFlatten));

    // Create accompanying step indices for reconstruction
    std::vector<uint32_t> indices;
    indices.resize(unitStepFlatten.size());
    std::ranges::copy(std::views::keys(unitStepFlatten), indices.begin());

    return { unitsSoA, indices };
}

/**
 * @brief Transform instance-major unit data back to time-major
 * @tparam UnitSoAT
 * @param flattenedUnits instance-major data to transform
 * @return Unit data grouped by time
 */
template<IsSoAType UnitSoAT>
[[nodiscard]] auto recoverFlattenedSortedUnits(
    const FlattenedUnits<UnitSoAT> &flattenedUnits) noexcept -> std::vector<std::vector<typename UnitSoAT::struct_type>>
{
    // Create outer dimension with the maximum game step index
    std::vector<std::vector<typename UnitSoAT::struct_type>> replayUnits;
    const std::size_t maxStepIdx = std::ranges::max(flattenedUnits.indices);
    replayUnits.resize(maxStepIdx + 1ull);

    // Copy units to correct timestep
    const auto &indices = flattenedUnits.indices;
    for (std::size_t idx = 0; idx < indices.size(); ++idx) {
        replayUnits[indices[idx]].emplace_back(flattenedUnits.units[idx]);
    }
    return replayUnits;
}


// -----------------------------------------------------------------
// FLATTENING VERSION 3
// -----------------------------------------------------------------


/**
 * @brief Start and count parameters for IOTA
 */
struct IotaRange
{
    std::uint32_t start;
    std::uint32_t num;
};

/**
 * @brief Flattened units in SoA form with associated step index
 * @tparam UnitSoAT Type of flattened unit
 */
template<typename UnitSoAT> struct FlattenedUnits2
{
    UnitSoAT units;
    std::vector<IotaRange> step_count;
    std::uint32_t max_step;
};

/**
 * @brief Second version of instance-major unit sorting, chunks the time indices into iota-ranges rather than spelling
 * out [1,2,3,...,N]. Saves a small amount of space, but the work is done so might as well use it.
 *
 * @tparam UnitSoAT unit structure type
 * @param replayUnits unit data to rearrange
 * @return FlattenedUnits<UnitSoAT> instance-major units
 */
template<IsSoAType UnitSoAT>
[[nodiscard]] auto flattenAndSortUnits2(
    const std::vector<std::vector<typename UnitSoAT::struct_type>> &replayUnits) noexcept -> FlattenedUnits2<UnitSoAT>
{
    using UnitT = UnitSoAT::struct_type;
    using UnitStepT = std::pair<std::uint32_t, UnitT>;
    std::vector<UnitStepT> unitStepFlatten;
    for (std::size_t idx = 0; idx < replayUnits.size(); ++idx) {
        std::ranges::transform(
            replayUnits[idx], std::back_inserter(unitStepFlatten), [=](UnitT u) { return std::make_pair(idx, u); });
    }

    // Significantly better compressibility when sorted by unit (and implicitly time)
    std::ranges::stable_sort(
        unitStepFlatten, [](const UnitStepT &a, const UnitStepT &b) { return a.second.id < b.second.id; });

    // Create flattened SoA
    FlattenedUnits2<UnitSoAT> result;
    result.max_step = replayUnits.size();
    result.units = cvt::AoStoSoA(std::views::values(unitStepFlatten));
    if (unitStepFlatten.empty()) {
        result.step_count.clear();
        return result;
    }

    // Create accompanying first step seen for reconstruction
    // std::vector<IotaRange> test;
    // for (auto &&iota_steps : std::views::keys(unitStepFlatten)
    //                              | std::views::chunk_by([](std::uint32_t p, std::uint32_t n) { return p + 1 == n; }))
    //                              {
    //     test.emplace_back(iota_steps.front(), static_cast<std::uint32_t>(std::ranges::size(iota_steps)));
    // }

    // Iterator impl of chunk-by
    auto start = unitStepFlatten.begin();
    auto end = unitStepFlatten.begin();
    for (; end != std::prev(unitStepFlatten.end()); ++end) {
        // Check if we're at the end the end of our chunk
        const auto next = std::next(end, 1);
        if (end->first + 1 != next->first) {
            result.step_count.emplace_back(start->first, static_cast<std::uint32_t>(std::distance(start, end) + 1));
            start = next;// set start to next chunk
        }
    }
    result.step_count.emplace_back(
        start->first, static_cast<std::uint32_t>(std::distance(start, end) + 1));// Handle last chunk

    return result;
}

/**
 * @brief Transform v2 instance-major unit data back to time-major
 * @tparam UnitSoAT
 * @param flattenedUnits instance-major data to transform
 * @return Unit data grouped by time
 */
template<IsSoAType UnitSoAT>
[[nodiscard]] auto recoverFlattenedSortedUnits2(const FlattenedUnits2<UnitSoAT> &flattenedUnits) noexcept
    -> std::vector<std::vector<typename UnitSoAT::struct_type>>
{
    // Create outer dimension with the maximum game step index
    std::vector<std::vector<typename UnitSoAT::struct_type>> replayUnits;
    replayUnits.resize(flattenedUnits.max_step);

    // Copy units to correct timestep
    const std::size_t num_units = flattenedUnits.units.id.size();
    std::uint32_t offset = 0;
    const auto &unit_ids = flattenedUnits.units.id;
    auto step_count = flattenedUnits.step_count.begin();
    for (std::size_t unit_idx = 0; unit_idx < num_units; ++unit_idx) {
        replayUnits[step_count->start + offset].emplace_back(flattenedUnits.units[unit_idx]);
        // Increment the base and reset the offset if there is a new unit next
        if (offset < step_count->num - 1) {
            offset++;
        } else {
            step_count++;
            offset = 0;
        }
    }
    return replayUnits;
}

}// namespace cvt
