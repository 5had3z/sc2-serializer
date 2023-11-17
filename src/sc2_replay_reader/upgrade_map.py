"""Maps from ability id to name grouped by race"""
from dataclasses import dataclass
from pathlib import Path
from itertools import product

from pysc2.lib.actions import FUNCTIONS
import yaml


_game_info_file = Path(__file__).parent / "unit_infos.yaml"

with open(_game_info_file, "r", encoding="utf-8") as f:
    _game_data = yaml.safe_load(f)
_version_mapping: dict[str, dict[str, int]] = {}
for _game_version in _game_data:
    _version_mapping[_game_version["version"]] = {
        u["name"]: u["ability_id"] for u in _game_version["upgrades"]
    }

_levels = ["1", "2", "3"]


def _str_tf(x):
    """Add prefix and posfix for research action"""
    # return f"Research_{x}_quick"
    return x


# --- Protoss ---
def _gen_protoss():
    entries = [
        "Charge",
        "ObserverGraviticBooster",
        "GraviticDrive",
        # "FluxVanes", # On wiki but not sc2api
        "AdeptResonatingGlaives",
        "PhoenixAnionPulseCrystals",
        "ExtendedThermalLance",
        "PsiStorm",
        "Blink",
        # "Hallucination", # On wiki but not sc2api
        "ShadowStrike",  # On wiki as ShadowStride which is the ability name
        "WarpGate",
        # "TectonicDestabilizers", # On wiki but not sc2 api
        "InterceptorGravitonCatapult",
    ]
    for a, b, c in product(["Ground", "Air"], ["Armor", "Weapons"], _levels):
        entries.append(f"Protoss{a}{b}Level{c}")
    entries.extend([f"ProtossShieldsLevel{l}" for l in _levels])
    entries = [_str_tf(e) for e in entries]
    return entries


def _protoss_remap():
    remap: dict[int, list[int]] = {}

    def remap_str(base: str):
        src = FUNCTIONS[_str_tf(base)].ability_id
        dst = [FUNCTIONS[_str_tf(f"{base}Level{l}")].ability_id for l in _levels]
        remap[src] = dst

    for a, b in product(["Ground", "Air"], ["Armor", "Weapons"]):
        remap_str(f"Protoss{a}{b}")
    remap_str("ProtossShields")

    return remap


# Protoss Research ID and Name mapping
PROTOSS: dict[int, str] = {_version_mapping[_key][p]: p for p in _gen_protoss()}
# Protoss Research ID to State Feature Index Mapping
PROTOSS_IDX: dict[int, int] = {
    _id: idx for idx, _id in enumerate(sorted(PROTOSS.keys()))
}
# Protoss remap non-leveled action to leveled
PROTOSS_REMAP: dict[int, list[int]] = _protoss_remap()


# --- Terran ---
def _gen_terran():
    entries = [
        # "HurricaneThrusters",
        "BansheeHyperflightRotors",
        "SmartServos",
        "CycloneRapidFireLaunchers",
        # "RapidReignitionSystem",
        # "NitroPacks",
        "AdvancedBallistics",
        "EnhancedShockwaves",
        "HiSecAutoTracking",
        # "EnhancedMunitions",
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
        # "MedivacCaduceusReactor",
        # "MoebiusReactor",
        # "TransformationServos",
        # "BehemothReactor",
        "CombatShield",
        "InfernalPreigniter",
        # "NeosteelArmor",
        "TerranStructureArmorUpgrade",
        # "DurableMaterials",
        "NeosteelFrame",
    ]

    # Weapons
    for a, c in product(["Infantry", "Vehicle", "Ship"], _levels):
        entries.append(f"Terran{a}WeaponsLevel{c}")
    # Plating
    entries.extend([f"TerranInfantryArmorLevel{l}" for l in _levels])
    entries.extend([f"TerranVehicleAndShipPlatingLevel{l}" for l in _levels])
    entries = [_str_tf(e) for e in entries]
    return entries


def _terran_remap():
    remap: dict[int, list[int]] = {}

    def remap_str(base: str):
        src = FUNCTIONS[_str_tf(base)].ability_id
        dst = [FUNCTIONS[_str_tf(f"{base}Level{l}")].ability_id for l in _levels]
        remap[src] = dst

    for a in ["Infantry", "Vehicle", "Ship"]:
        remap_str(f"Terran{a}Weapons")
    remap_str("TerranInfantryArmor")
    remap_str("TerranVehicleAndShipPlating")

    return remap


# Terran Research ID and Name mapping
TERRAN: dict[int, str] = {FUNCTIONS[p].ability_id: p for p in _gen_terran()}
# Terran Research ID to State Feature Index Mapping
TERRAN_IDX: dict[int, int] = {_id: idx for idx, _id in enumerate(sorted(TERRAN.keys()))}
# Terran remap non-leveled action to leveled
TERRAN_REMAP: dict[int, list[int]] = _terran_remap()


# --- Zerg ---
def _gen_zerg():
    entries = [
        # "ChitinousPlating",
        "AdaptiveTalons",
        "AnabolicSynthesis",
        "CentrifugalHooks",
        "GlialRegeneration",  # Wiki is Reconstitution
        "ZerglingMetabolicBoost",
        "PneumatizedCarapace",
        "MuscularAugments",
        "GroovedSpines",
        # "SeismicSpines",
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
    entries = [_str_tf(e) for e in entries]
    return entries


def _zerg_remap():
    remap: dict[int, list[int]] = {}

    def remap_str(base: str):
        src = FUNCTIONS[_str_tf(base)].ability_id
        dst = [FUNCTIONS[_str_tf(f"{base}Level{l}")].ability_id for l in _levels]
        remap[src] = dst

    for a in ["Melee", "Missile"]:
        remap_str(f"Zerg{a}Weapons")
    remap_str("ZergFlyerAttack")
    for a in ["Ground", "Flyer"]:
        remap_str(f"Zerg{a}Armor")

    return remap


# Zerg Research ID and Name mapping
ZERG: dict[int, str] = {FUNCTIONS[p].ability_id: p for p in _gen_zerg()}
# Zerg Research ID to State Feature Index Mapping
ZERG_IDX: dict[int, int] = {_id: idx for idx, _id in enumerate(sorted(ZERG.keys()))}
# Zerg remap non-leveled action to leveled
ZERG_REMAP: dict[int, list[int]] = _zerg_remap()


@dataclass
class GameUpgradeInfo:
    protoss: dict[int, str]
    terran: dict[int, str]
    zerg: dict[int, str]

    @classmethod
    def from_upgrades(cls, upgrades: dict[str, int]):
        """"""
        _zerg = {upgrades[p]: p for p in _gen_zerg()}
        _terran = {upgrades[p]: p for p in _gen_terran()}
        _protoss = {upgrades[p]: p for p in _gen_protoss()}
        return cls(_protoss, _terran, _zerg)


# Mapping from game version to upgrade info
UPGRADE_INFO: dict[str, GameUpgradeInfo] = {}

for _version, _upgrades in _version_mapping.items():
    UPGRADE_INFO[_version] = GameUpgradeInfo.from_upgrades(_upgrades)
