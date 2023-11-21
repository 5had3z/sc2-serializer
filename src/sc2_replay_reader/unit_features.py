from enum import IntEnum, auto

import sc2_replay_reader as sc2

_VisSize = len(sc2.Visibility.__entries)
_AliSize = len(sc2.Alliance.__entries)
_ClkSize = len(sc2.CloakState.__entries)


class Unit(IntEnum):
    """SC2 Unit Feature Indices"""

    id = 0
    tgtId = auto()
    observation = auto()
    alliance = auto()
    cloak_state = auto()
    unitType = auto()
    health = auto()
    health_max = auto()
    shield = auto()
    shield_max = auto()
    energy = auto()
    energy_max = auto()
    cargo = auto()
    cargo_max = auto()
    assigned_harvesters = auto()
    ideal_harvesters = auto()
    weapon_cooldown = auto()
    x = auto()
    y = auto()
    z = auto()
    t = auto()
    r = auto()
    build_prog = auto()
    is_blip = auto()
    is_flying = auto()
    is_burrowed = auto()
    is_powered = auto()
    in_cargo = auto()
    order0 = auto()
    order1 = auto()
    order2 = auto()
    order3 = auto()
    buff0 = auto()
    buff1 = auto()
    add_on_tag = auto()


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
    unitType = auto()
    health = auto()
    health_max = auto()
    shield = auto()
    shield_max = auto()
    energy = auto()
    energy_max = auto()
    cargo = auto()
    cargo_max = auto()
    assigned_harvesters = auto()
    ideal_harvesters = auto()
    weapon_cooldown = auto()
    x = auto()
    y = auto()
    z = auto()
    t = auto()
    r = auto()
    build_prog = auto()
    is_blip = auto()
    is_flying = auto()
    is_burrowed = auto()
    is_powered = auto()
    in_cargo = auto()
    order0 = auto()
    order1 = auto()
    order2 = auto()
    order3 = auto()
    buff0 = auto()
    buff1 = auto()
    add_on_tag = auto()


class NeutralUnit(IntEnum):
    """SC2 Neutral Unit Feature Indices"""

    id = 0
    unitType = auto()
    obs = auto()
    health = auto()
    health_max = auto()
    x = auto()
    y = auto()
    z = auto()
    t = auto()
    r = auto()
    content = auto()


class NeutralUnitOH(IntEnum):
    """SC2 Neutral Unit Feature Indices with OneHot Enums"""

    id = 0
    unitType = auto()
    visible = auto()
    snapshot = auto()
    hidden = auto()
    health = auto()
    health_max = auto()
    x = auto()
    y = auto()
    z = auto()
    t = auto()
    r = auto()
    content = auto()


if __name__ == "__main__":
    assert UnitOH.alliance_self == Unit.alliance + _VisSize - 1
    assert UnitOH.unitType == Unit.unitType + _AliSize + _ClkSize + _VisSize - 3
    assert NeutralUnitOH.x == NeutralUnit.x + _VisSize - 1
