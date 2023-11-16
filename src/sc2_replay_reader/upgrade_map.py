"""Maps from ability id to name grouped by race"""

from itertools import product

from pysc2.lib.actions import FUNCTIONS

_levels = ["1", "2", "3"]


def _str_tf(x):
    """Add prefix and posfix for research action"""
    return f"Research_{x}_quick"


# --- Protoss ---
def _gen_protoss():
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
PROTOSS: dict[int, str] = {FUNCTIONS[p].ability_id: p for p in _gen_protoss()}
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
        # "CaduceusReactor",
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
