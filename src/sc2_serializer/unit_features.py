"""Enum helpers to index into unit feature vectors by name"""

from enum import IntEnum, auto

import sc2_serializer as sc2

_VisSize = len(sc2.Visibility.__entries)
_AliSize = len(sc2.Alliance.__entries)
_ClkSize = len(sc2.CloakState.__entries)
_AddonSize = len(sc2.AddOn.__entries)


class Unit(IntEnum):
    """SC2 Unit Feature Indices"""

    id = 0
    tgtId = auto()
    observation = auto()
    alliance = auto()
    cloak_state = auto()
    add_on_tag = auto()
    unitType = auto()
    health = auto()
    health_max = auto()
    shield = auto()
    shield_max = auto()
    energy = auto()
    energy_max = auto()
    weapon_cooldown = auto()
    buff0 = auto()
    buff1 = auto()
    x = auto()
    y = auto()
    z = auto()
    t = auto()
    r = auto()
    build_prog = auto()
    cargo = auto()
    cargo_max = auto()
    assigned_harvesters = auto()
    ideal_harvesters = auto()
    is_blip = auto()
    is_flying = auto()
    is_burrowed = auto()
    is_powered = auto()
    in_cargo = auto()
    order0_ability_id = auto()
    order0_progress = auto()
    order0_target_id = auto()
    order0_target_x = auto()
    order0_target_y = auto()
    order1_ability_id = auto()
    order1_progress = auto()
    order1_target_id = auto()
    order1_target_x = auto()
    order1_target_y = auto()
    order2_ability_id = auto()
    order2_progress = auto()
    order2_target_id = auto()
    order2_target_x = auto()
    order2_target_y = auto()
    order3_ability_id = auto()
    order3_progress = auto()
    order3_target_id = auto()
    order3_target_x = auto()
    order3_target_y = auto()


class UnitOH(IntEnum):
    """SC2 Unit Feature Indices with OneHot Enums"""

    id = 0
    tgtId = auto()
    visible = auto()
    snapshot = auto()
    hidden = auto()
    alliance_self = auto()
    alliance_friend = auto()
    alliance_neutral = auto()
    alliance_enemy = auto()
    cloak_unknown = auto()
    cloak_cloaked = auto()
    cloak_detected = auto()
    cloak_uncloaked = auto()
    cloak_allied = auto()
    addon_none = auto()
    addon_reactor = auto()
    addon_techlab = auto()
    unitType = auto()
    health = auto()
    health_max = auto()
    shield = auto()
    shield_max = auto()
    energy = auto()
    energy_max = auto()
    weapon_cooldown = auto()
    buff0 = auto()
    buff1 = auto()
    x = auto()
    y = auto()
    z = auto()
    t = auto()
    r = auto()
    build_prog = auto()
    cargo = auto()
    cargo_max = auto()
    assigned_harvesters = auto()
    ideal_harvesters = auto()
    is_blip = auto()
    is_flying = auto()
    is_burrowed = auto()
    is_powered = auto()
    in_cargo = auto()
    order0_ability_id = auto()
    order0_progress = auto()
    order0_target_id = auto()
    order0_target_x = auto()
    order0_target_y = auto()
    order1_ability_id = auto()
    order1_progress = auto()
    order1_target_id = auto()
    order1_target_x = auto()
    order1_target_y = auto()
    order2_ability_id = auto()
    order2_progress = auto()
    order2_target_id = auto()
    order2_target_x = auto()
    order2_target_y = auto()
    order3_ability_id = auto()
    order3_progress = auto()
    order3_target_id = auto()
    order3_target_x = auto()
    order3_target_y = auto()


class NeutralUnit(IntEnum):
    """SC2 Neutral Unit Feature Indices"""

    id = 0
    unitType = auto()
    health = auto()
    health_max = auto()
    x = auto()
    y = auto()
    z = auto()
    t = auto()
    r = auto()
    content = auto()
    obs = auto()


class NeutralUnitOH(IntEnum):
    """SC2 Neutral Unit Feature Indices with OneHot Enums"""

    id = 0
    unitType = auto()
    health = auto()
    health_max = auto()
    x = auto()
    y = auto()
    z = auto()
    t = auto()
    r = auto()
    content = auto()
    visible = auto()
    snapshot = auto()
    hidden = auto()


if __name__ == "__main__":
    assert UnitOH.alliance_self == Unit.alliance + _VisSize - 1
    assert (
        UnitOH.unitType
        == Unit.unitType + _AliSize + _ClkSize + _VisSize + _AddonSize - 4
    )
    assert NeutralUnitOH.x == NeutralUnit.x
