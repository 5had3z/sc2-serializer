from enum import IntEnum, auto

import sc2_replay_reader as sc2

_VisSize = len(sc2.Visibility.__entries)
_AliSize = len(sc2.Alliance.__entries)
_ClkSize = len(sc2.CloakState.__entries)


class Unit(IntEnum):
    """SC2 Unit Feature Indices"""

    @staticmethod
    def _generate_next_value_(
        name: str, start: int, count: int, last_values: list[int]
    ):
        """Start at index zero"""
        return count

    id = auto()
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
    x = auto()
    y = auto()
    z = auto()
    t = auto()
    r = auto()
    build_prog = auto()
    is_blip = auto()
    is_burrowed = auto()
    is_powered = auto()


class UnitOH(IntEnum):
    """SC2 Unit Feature Indices with OneHot Enums"""

    @staticmethod
    def _generate_next_value_(
        name: str, start: int, count: int, last_values: list[int]
    ):
        """Need to expand onehot index"""
        if count == 0:
            return 0
        if name == "alliance":
            return last_values[-1] + _VisSize
        if name == "cloak_state":
            return last_values[-1] + _AliSize
        if name == "unitType":
            return last_values[-1] + _ClkSize
        return last_values[-1] + 1

    id = auto()
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
    x = auto()
    y = auto()
    z = auto()
    t = auto()
    r = auto()
    build_prog = auto()
    is_blip = auto()
    is_burrowed = auto()
    is_powered = auto()


class NeutralUnit(IntEnum):
    """SC2 Neutral Unit Feature Indices"""

    @staticmethod
    def _generate_next_value_(
        name: str, start: int, count: int, last_values: list[int]
    ):
        """Start at index zero"""
        return count

    id = auto()
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

    @staticmethod
    def _generate_next_value_(
        name: str, start: int, count: int, last_values: list[int]
    ):
        """Need to expand onehot index"""
        if count == 0:
            return 0
        if name == "health":
            return last_values[-1] + _VisSize
        return last_values[-1] + 1

    id = auto()
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


if __name__ == "__main__":
    assert UnitOH.alliance == Unit.alliance + _VisSize - 1
    assert UnitOH.unitType == Unit.unitType + _AliSize + _ClkSize + _VisSize - 3
    assert NeutralUnitOH.x == NeutralUnit.x + _VisSize - 1
