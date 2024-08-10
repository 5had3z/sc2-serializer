/**
 * @file units.hpp
 * @author Bryce Ferenczi
 * @brief Unit data structures for StarCraft II. NeutralUnits are treated specially as many properties such as buffs and
 * cloak_state are never utilized, so we save precious bytes and make a smaller structure especially for neutral
 * structures.
 * @version 0.1
 * @date 2024-05-27
 *
 * @copyright Copyright (c) 2024
 *
 */
#pragma once

#include "common.hpp"
#include "enums.hpp"
#include "soa.hpp"

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

    [[nodiscard]] inline auto size() const noexcept -> std::size_t { return id.size(); }

    /**
     * @brief Gather unit data at index
     * @param idx index in SoA to gather data from
     * @return Unit
     */
    [[nodiscard]] auto operator[](std::size_t idx) const noexcept -> Unit { return gatherStructAtIndex(*this, idx); }
};


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

    [[nodiscard]] inline auto size() const noexcept -> std::size_t { return id.size(); }

    [[nodiscard]] auto operator[](std::size_t idx) const noexcept -> NeutralUnit
    {
        return gatherStructAtIndex(*this, idx);
    }
};

}// namespace cvt
