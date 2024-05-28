/**
 * @file observer_utils.cpp
 * @author Bryce Ferenczi
 * @brief Implementations of observer utility functions.
 * @version 0.1
 * @date 2024-05-28
 *
 * @copyright Copyright (c) 2024
 *
 */

#include "observer_utils.hpp"

namespace cvt {

/**
 * @brief Finds the tagged unit with the specified add-on tag in the given units.
 *
 * @param add_on_tag The tag of the add-on to search for.
 * @param units The list of units to search in.
 * @return The found AddOn unit.
 */
[[nodiscard]] auto findTaggedUnit(const sc2::Tag add_on_tag, const sc2::Units &units) -> AddOn
{
    auto same_tag = [add_on_tag](const sc2::Unit *other) { return other->tag == add_on_tag; };
    const auto it = std::ranges::find_if(units, same_tag);
    if (it == units.end()) {
        throw std::out_of_range(fmt::format("Tagged unit was not found!", static_cast<int>(add_on_tag)));
    } else {
        const sc2::UNIT_TYPEID type = (*it)->unit_type.ToType();

        const std::unordered_set<sc2::UNIT_TYPEID> techlabs = { sc2::UNIT_TYPEID::TERRAN_BARRACKSTECHLAB,
            sc2::UNIT_TYPEID::TERRAN_FACTORYTECHLAB,
            sc2::UNIT_TYPEID::TERRAN_STARPORTTECHLAB,
            sc2::UNIT_TYPEID::TERRAN_TECHLAB };

        if (techlabs.contains(type)) { return AddOn::TechLab; }

        const std::unordered_set<sc2::UNIT_TYPEID> reactors = { sc2::UNIT_TYPEID::TERRAN_BARRACKSREACTOR,
            sc2::UNIT_TYPEID::TERRAN_FACTORYREACTOR,
            sc2::UNIT_TYPEID::TERRAN_STARPORTREACTOR,
            sc2::UNIT_TYPEID::TERRAN_REACTOR };

        if (reactors.contains(type)) { return AddOn::Reactor; }

        throw std::out_of_range(fmt::format("Invalid Add On Type, type was {}!", static_cast<int>(type)));
    }
}

[[nodiscard]] auto convertSC2UnitOrder(const sc2::UnitOrder *src) noexcept -> UnitOrder
{
    UnitOrder dst;
    static_assert(std::is_same_v<std::underlying_type_t<sc2::ABILITY_ID>, int>);
    dst.ability_id = static_cast<int>(src->ability_id);
    dst.progress = src->progress;
    dst.tgtId = src->target_unit_tag;
    dst.target_pos.x = src->target_pos.x;
    dst.target_pos.y = src->target_pos.y;
    return dst;
}

[[nodiscard]] auto convertSC2Unit(const sc2::Unit *src, const sc2::Units &units, const bool isPassenger) -> Unit
{
    Unit dst{};
    dst.id = src->tag;
    dst.unitType = src->unit_type;
    dst.observation = static_cast<Visibility>(src->display_type);
    dst.alliance = static_cast<Alliance>(src->alliance);// Enums deffs match here
    dst.health = src->health;
    dst.health_max = src->health_max;
    dst.shield = src->shield;
    dst.shield_max = src->shield_max;
    dst.energy = src->energy_max;
    dst.energy_max = src->energy_max;
    dst.cargo = src->cargo_space_taken;
    dst.cargo_max = src->cargo_space_max;
    dst.assigned_harvesters = src->assigned_harvesters;
    dst.ideal_harvesters = src->ideal_harvesters;
    dst.weapon_cooldown = src->weapon_cooldown;
    dst.tgtId = src->engaged_target_tag;
    dst.cloak_state = static_cast<CloakState>(src->cloak);// These should match
    dst.is_blip = src->is_blip;
    dst.is_flying = src->is_flying;
    dst.is_burrowed = src->is_burrowed;
    dst.is_powered = src->is_powered;
    dst.in_cargo = isPassenger;
    dst.pos.x = src->pos.x;
    dst.pos.y = src->pos.y;
    dst.pos.z = src->pos.z;
    dst.heading = src->facing;
    dst.radius = src->radius;
    dst.build_progress = src->build_progress;

    if (src->orders.size() >= 1) { dst.order0 = convertSC2UnitOrder(&src->orders[0]); }
    if (src->orders.size() >= 2) { dst.order1 = convertSC2UnitOrder(&src->orders[1]); }
    if (src->orders.size() >= 3) { dst.order2 = convertSC2UnitOrder(&src->orders[2]); }
    if (src->orders.size() >= 4) { dst.order3 = convertSC2UnitOrder(&src->orders[3]); }

    static_assert(std::is_same_v<std::underlying_type_t<sc2::BUFF_ID>, int>);
    if (src->buffs.size() >= 1) { dst.buff0 = static_cast<int>(src->buffs[0]); }
    if (src->buffs.size() >= 2) { dst.buff1 = static_cast<int>(src->buffs[1]); }
    if (src->add_on_tag != 0) { dst.add_on_tag = findTaggedUnit(src->add_on_tag, units); }

    return dst;
}

[[nodiscard]] auto convertSC2NeutralUnit(const sc2::Unit *src) noexcept -> NeutralUnit
{
    NeutralUnit dst{};
    dst.id = src->tag;
    dst.unitType = src->unit_type;
    dst.observation = static_cast<Visibility>(src->display_type);
    dst.health = src->health;
    dst.health_max = src->health_max;
    dst.pos.x = src->pos.x;
    dst.pos.y = src->pos.y;
    dst.pos.z = src->pos.z;
    dst.heading = src->facing;
    dst.radius = src->radius;
    dst.contents = std::max(src->vespene_contents, src->mineral_contents);
    return dst;
}

[[nodiscard]] auto convertScore(const sc2::Score *src) -> Score
{
    if (src->score_type != sc2::ScoreType::Melee) {
        throw std::runtime_error(fmt::format("Score type is not melee, got {}", static_cast<int>(src->score_type)));
    };
    Score dst{};

    dst.score_float = src->score;
    dst.idle_production_time = src->score_details.idle_production_time;
    dst.idle_worker_time = src->score_details.idle_worker_time;
    dst.total_value_units = src->score_details.total_value_units;
    dst.total_value_structures = src->score_details.total_value_structures;
    dst.killed_value_units = src->score_details.killed_value_units;
    dst.killed_value_structures = src->score_details.killed_value_structures;
    dst.collected_minerals = src->score_details.collected_minerals;
    dst.collected_vespene = src->score_details.collected_vespene;
    dst.collection_rate_minerals = src->score_details.collection_rate_minerals;
    dst.collection_rate_vespene = src->score_details.collection_rate_vespene;
    dst.spent_minerals = src->score_details.spent_minerals;
    dst.spent_vespene = src->score_details.spent_vespene;

    dst.total_damage_dealt_life = src->score_details.total_damage_dealt.life;
    dst.total_damage_dealt_shields = src->score_details.total_damage_dealt.shields;
    dst.total_damage_dealt_energy = src->score_details.total_damage_dealt.energy;

    dst.total_damage_taken_life = src->score_details.total_damage_taken.life;
    dst.total_damage_taken_shields = src->score_details.total_damage_taken.shields;
    dst.total_damage_taken_energy = src->score_details.total_damage_taken.energy;

    dst.total_healed_life = src->score_details.total_healed.life;
    dst.total_healed_shields = src->score_details.total_healed.shields;
    dst.total_healed_energy = src->score_details.total_healed.energy;

    return dst;
}

}// namespace cvt
