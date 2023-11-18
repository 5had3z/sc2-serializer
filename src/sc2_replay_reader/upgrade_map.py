"""Maps from ability id to name grouped by race"""
from dataclasses import dataclass, field
from itertools import product
from pathlib import Path

import yaml
from pysc2.lib.actions import FUNCTIONS

# Read game info file to load action/upgrade data
_game_info_file = Path(__file__).parent / "game_info.yaml"
with open(_game_info_file, "r", encoding="utf-8") as f:
    _game_data = yaml.safe_load(f)
_version_mapping: dict[str, dict[str, int]] = {}
for _game_version in _game_data:
    # Use Name, Friendly Name and PySC2 Name to try and get a match
    _version_mapping[_game_version["version"]] = {
        u["name"]: u["ability_id"] for u in _game_version["upgrades"]
    }
    _version_mapping[_game_version["version"]].update(
        {u["friendly_name"]: u["ability_id"] for u in _game_version["upgrades"]}
    )
    _version_mapping[_game_version["version"]].update(
        {u["pysc2_name"]: u["ability_id"] for u in _game_version["upgrades"]}
    )
    # Hail mary just fucking use pysc2 directly as well
    _version_mapping[_game_version["version"]].update(
        {u.name: u.ability_id for u in FUNCTIONS}
    )

_levels = ["1", "2", "3"]


# --- Protoss ---
def _gen_protoss():
    """List of protoss upgrade names"""
    entries = [
        "Charge",
        "GraviticBooster",
        "GraviticDrive",
        # "FluxVanes", # On wiki but not sc2api
        "AdeptResonatingGlaives",
        "PhoenixAnionPulseCrystals",
        "ExtendedThermalLance",
        "PsiStorm",
        "Blink",
        "Hallucination",  # On wiki but not sc2api
        "ShadowStrike",  # On wiki as ShadowStride which is the ability name
        "WarpGate",
        # "TectonicDestabilizers", # On wiki but not sc2 api
        "InterceptorLaunchSpeedUpgrade",  # InterceptorGravitonCatapult
    ]
    for a, b, c in product(["Ground", "Air"], ["Armor", "Weapons"], _levels):
        entries.append(f"Protoss{a}{b}Level{c}")
    entries.extend([f"ProtossShieldsLevel{l}" for l in _levels])
    return entries


# --- Terran ---
def _gen_terran():
    """List of terran upgrade names"""
    entries = [
        # "HurricaneThrusters",
        "BansheeHyperflightRotors",
        "SmartServos",
        "CycloneRapidFireLaunchers",
        # "RapidReignitionSystem",
        # "NitroPacks",
        "AdvancedBallistics",
        # "EnhancedShockwaves",
        "HiSecAutoTracking",
        "RavenEnhancedMunitions",
        # "MagFieldLaunchers",
        # "MagFieldAccelerator",
        "RavenRecalibratedExplosives",
        "BansheeCloakingField",
        "ConcussiveShells",
        "PersonalCloaking",
        "Stimpack",
        "BattlecruiserWeaponRefit",
        "DrillingClaws",
        "RavenCorvidReactor",
        "MedivacCaduceusReactor",
        "GhostMoebiusReactor",
        "TransformationServos",
        # "BehemothReactor",
        "CombatShield",
        "InfernalPreigniter",
        # "NeosteelArmor",
        "TerranStructureArmorUpgrade",
        "DurableMaterials",
        "NeosteelFrame",
    ]

    # Weapons
    for a, c in product(["Infantry", "Vehicle", "Ship"], _levels):
        entries.append(f"Terran{a}WeaponsLevel{c}")
    # Plating
    entries.extend([f"TerranInfantryArmorLevel{l}" for l in _levels])
    entries.extend([f"TerranVehicleAndShipPlatingLevel{l}" for l in _levels])
    return entries


# --- Zerg ---
def _gen_zerg():
    """List of zerg upgrade names"""
    entries = [
        "ChitinousPlating",
        "AdaptiveTalons",
        "AnabolicSynthesis",
        "CentrifugalHooks",
        "GlialRegeneration",  # Wiki is Reconstitution
        "ZerglingMetabolicBoost",
        "PneumatizedCarapace",
        "MuscularAugments",
        "GroovedSpines",
        "LurkerRange",  # SeismicSpines
        "Burrow",
        "NeuralParasite",
        # "EvolveMicrobialShroud",
        "PathogenGlands",
        # "VentralSacs",
        "ZerglingAdrenalGlands",
        "TunnelingClaws",
        # "FlyingLocusts",
    ]

    # Melee Missile and Flyer Attack
    for a, l in product(["Melee", "Missile"], _levels):
        entries.append(f"Zerg{a}WeaponsLevel{l}")
    entries.extend([f"ZergFlyerAttackLevel{l}" for l in _levels])
    # Ground and flyer armor
    for a, l in product(["Ground", "Flyer"], _levels):
        entries.append(f"Zerg{a}ArmorLevel{l}")
    return entries


@dataclass
class GameUpgradeInfo:
    """Upgrade ActionID to Name grouped by Race"""

    protoss: dict[int, str]
    terran: dict[int, str]
    zerg: dict[int, str]

    protoss_lvl_remap: dict[int, list[int]] = field(default_factory=dict)
    terran_lvl_remap: dict[int, list[int]] = field(default_factory=dict)
    zerg_lvl_remap: dict[int, list[int]] = field(default_factory=dict)

    @classmethod
    def from_upgrades(cls, upgrades: dict[str, int]):
        """Create from Upgrade Name to ActionID Dict"""
        _zerg = {upgrades[p]: p for p in _gen_zerg()}
        _terran = {upgrades[p]: p for p in _gen_terran()}
        _protoss = {upgrades[p]: p for p in _gen_protoss()}
        return cls(_protoss, _terran, _zerg)


# Mapping from game version to action/upgrade info
UPGRADE_INFO: dict[str, GameUpgradeInfo] = {}

for _version, _upgrades in _version_mapping.items():
    UPGRADE_INFO[_version] = GameUpgradeInfo.from_upgrades(_upgrades)

    def _protoss_remap():
        remap: dict[int, list[int]] = {}

        def remap_str(base: str):
            src = _upgrades[base]
            dst = [_upgrades[f"{base}Level{l}"] for l in _levels]
            remap[src] = dst

        for a, b in product(["Ground", "Air"], ["Armor", "Weapons"]):
            remap_str(f"Protoss{a}{b}")
        remap_str("ProtossShields")

        return remap

    UPGRADE_INFO[_version].protoss_lvl_remap.update(_protoss_remap())

    def _terran_remap():
        remap: dict[int, list[int]] = {}

        def remap_str(base: str):
            src = _upgrades[base]
            dst = [_upgrades[f"{base}Level{l}"] for l in _levels]
            remap[src] = dst

        for a in ["Infantry", "Vehicle", "Ship"]:
            remap_str(f"Terran{a}Weapons")
        remap_str("TerranInfantryArmor")
        remap_str("TerranVehicleAndShipPlating")

        return remap

    UPGRADE_INFO[_version].terran_lvl_remap.update(_terran_remap())

    def _zerg_remap():
        remap: dict[int, list[int]] = {}

        def remap_str(base: str):
            src = _upgrades[base]
            dst = [_upgrades[f"{base}Level{l}"] for l in _levels]
            remap[src] = dst

        for a in ["Melee", "Missile"]:
            remap_str(f"Zerg{a}Weapons")
        remap_str("ZergFlyerAttack")
        for a in ["Ground", "Flyer"]:
            remap_str(f"Zerg{a}Armor")

        return remap

    UPGRADE_INFO[_version].zerg_lvl_remap.update(_zerg_remap())
