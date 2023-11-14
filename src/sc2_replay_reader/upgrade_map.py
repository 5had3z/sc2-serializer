"""Maps from ability id to name grouped by race"""

from itertools import product

from pysc2.lib.actions import FUNCTIONS


# --- Protoss ---

protos_wiki_entries = [
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
protos_wiki_entries = [f"Research_{e}_quick" for e in protos_wiki_entries]


def generate_protoss_research():
    """Generate Ground|Air Armor|Weapons|Shields Level 1|2|3 for race"""
    entries = []
    levels = ["1", "2", "3"]
    for a, b, c in product(["Ground", "Air"], ["Armor", "Weapons"], levels):
        entries.append(f"Research_Protoss{a}{b}Level{c}_quick")
    entries.extend([f"Research_ProtossShieldsLevel{l}_quick" for l in levels])
    return entries


protos_wiki_entries.extend(generate_protoss_research())
# Create mapping
protoss_id_to_str = {FUNCTIONS[p].ability_id: p for p in protos_wiki_entries}

# --- Terran ---

terran_wiki_entries = [
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


def generate_terran_research():
    """Generate Ground|Air Armor|Weapons|Shields Level 1|2|3 for race"""
    entries = []
    levels = ["1", "2", "3"]
    # Weapons
    for a, c in product(["Infantry", "Vehicle", "Ship"], levels):
        entries.append(f"Research_Terran{a}WeaponsLevel{c}_quick")
    # Plating
    entries.extend([f"Research_TerranInfantryArmorLevel{l}_quick" for l in levels])
    entries.extend(
        [f"Research_TerranVehicleAndShipPlatingLevel{l}_quick" for l in levels]
    )
    return entries


terran_wiki_entries = [f"Research_{e}_quick" for e in terran_wiki_entries]
terran_wiki_entries.extend(generate_terran_research())
# Create mapping
terran_id_to_str = {FUNCTIONS[p].ability_id: p for p in terran_wiki_entries}


# --- Zerg ---


def generate_zerg_entries():
    entries = []
    levels = ["1", "2", "3"]

    # Melee Missle and Flyer Attack
    for a, l in product(["Melee", "Missile"], levels):
        entries.append(f"{a}WeaponsLevel{l}")
    entries.extend([f"FlyerAttackLevel{l}" for l in levels])

    # Ground and flyer armor
    for a, l in product(["Ground", "Flyer"], levels):
        entries.append(f"{a}ArmorLevel{l}")
    entries = [f"Research_Zerg{e}_quick" for e in entries]
    return entries


zerg_wiki_entries = [
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
zerg_wiki_entries = [f"Research_{e}_quick" for e in zerg_wiki_entries]
zerg_wiki_entries.extend(generate_zerg_entries())
# Create mapping
protoss_id_to_str = {FUNCTIONS[p].ability_id: p for p in zerg_wiki_entries}
