"""Maps from ability id to name grouped by race"""

from itertools import product

from pysc2.lib.actions import FUNCTIONS


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
    levels = ["1", "2", "3"]
    for a, b, c in product(["Ground", "Air"], ["Armor", "Weapons"], levels):
        entries.append(f"Protoss{a}{b}Level{c}")
    entries.extend([f"ProtossShieldsLevel{l}" for l in levels])
    entries = [f"Research_{e}_quick" for e in entries]
    return entries


# Protoss Research ID and Name mapping
PROTOSS: dict[int, str] = {FUNCTIONS[p].ability_id: p for p in _gen_protoss()}
# Protoss Research ID to State Feature Index Mapping
PROTOSS_IDX: dict[int, int] = {
    _id: idx for idx, _id in enumerate(sorted(PROTOSS.keys()))
}


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

    levels = ["1", "2", "3"]
    # Weapons
    for a, c in product(["Infantry", "Vehicle", "Ship"], levels):
        entries.append(f"Terran{a}WeaponsLevel{c}")
    # Plating
    entries.extend([f"TerranInfantryArmorLevel{l}" for l in levels])
    entries.extend([f"TerranVehicleAndShipPlatingLevel{l}" for l in levels])

    entries = [f"Research_{e}_quick" for e in entries]
    return entries


# Terran Research ID and Name mapping
TERRAN: dict[int, str] = {FUNCTIONS[p].ability_id: p for p in _gen_terran()}
# Terran Research ID to State Feature Index Mapping
TERRAN_IDX: dict[int, int] = {_id: idx for idx, _id in enumerate(sorted(TERRAN.keys()))}


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
    levels = ["1", "2", "3"]

    # Melee Missle and Flyer Attack
    for a, l in product(["Melee", "Missile"], levels):
        entries.append(f"Zerg{a}WeaponsLevel{l}")
    entries.extend([f"ZergFlyerAttackLevel{l}" for l in levels])

    # Ground and flyer armor
    for a, l in product(["Ground", "Flyer"], levels):
        entries.append(f"Zerg{a}ArmorLevel{l}")
    entries = [f"Research_{e}_quick" for e in entries]
    return entries


# Zerg Research ID and Name mapping
ZERG: dict[int, str] = {FUNCTIONS[p].ability_id: p for p in _gen_zerg()}
# Zerg Research ID to State Feature Index Mapping
ZERG_IDX: dict[int, int] = {_id: idx for idx, _id in enumerate(sorted(ZERG.keys()))}
