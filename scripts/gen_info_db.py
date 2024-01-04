#!/usr/bin/env python3
"""
Use PySC2 and run over each game version in a folder and write out
information about different upgrades.
"""
import os
from dataclasses import dataclass, field, asdict
from pathlib import Path
from typing import Iterable, Mapping

from absl import flags
from absl import app
import yaml
from pysc2 import maps
from pysc2.env import sc2_env
from pysc2.lib.actions import FUNCTIONS
from pysc2.run_configs.platforms import Linux as SC2Linux
from s2clientprotocol import sc2api_pb2 as sc_pb
from s2clientprotocol.data_pb2 import UpgradeData, AbilityData


@dataclass
class Unit:
    uid: int
    name: str
    mineral: int
    vespene: int
    time: int
    food: int


@dataclass
class Upgrade:
    uid: int
    ability_id: int
    name: str
    mineral: int
    vespene: int
    time: int
    button_name: str
    friendly_name: str
    pysc2_name: str


@dataclass
class GameInfo:
    version: str
    units: list[Unit] = field(default_factory=list)
    upgrades: list[Upgrade] = field(default_factory=list)


def make_creation_msg(game) -> sc_pb.RequestCreateGame:
    create = sc_pb.RequestCreateGame(realtime=False, disable_fog=False)
    create.player_setup.add(type=sc_pb.Participant)
    create.player_setup.add(
        type=sc_pb.Computer,
        race=sc2_env.Race["random"],
        difficulty=sc2_env.Difficulty["very_easy"],
        ai_build=sc2_env.BotBuild["random"],
    )
    map_inst = maps.get("Simple64")
    create.local_map.map_path = map_inst.path
    create.local_map.map_data = map_inst.data(game)
    return create


def make_interface_opts():
    return sc_pb.InterfaceOptions()


FUNCTIONS_BY_ID = {
    f.ability_id: f.name[len("Research_") : -len("_quick")] for f in list(FUNCTIONS)
}


def parse_upgrades(upgrades, abilities):
    """"""
    res: list[Upgrade] = []

    for upgrade in upgrades:
        ability = abilities[upgrade.ability_id]
        try:
            pysc2_name = FUNCTIONS_BY_ID[upgrade.ability_id]
        except KeyError:
            pysc2_name = ""
        res.append(
            Upgrade(
                upgrade.upgrade_id,
                upgrade.ability_id,
                upgrade.name,
                upgrade.mineral_cost,
                upgrade.vespene_cost,
                upgrade.research_time,
                ability.button_name,
                ability.friendly_name.replace("Research", "")
                .replace("Evolve", "")
                .strip(),
                pysc2_name,
            )
        )
    return res


def parse_upgrades2(
    abilities: Iterable[AbilityData], upgrades: Mapping[int, UpgradeData]
):
    """Run based off actions to handle non-leveled actions which aren't in the upgrades list"""
    res: list[Upgrade] = []

    _upgrade_map: dict[int, UpgradeData] = {u.ability_id: u for u in upgrades.values()}

    for ability in abilities:
        try:
            pysc2_name = FUNCTIONS_BY_ID[ability.ability_id]
        except KeyError:
            pysc2_name = ""
        try:
            _upgrade = _upgrade_map[ability.ability_id]
        except KeyError:
            _upgrade = UpgradeData()
        res.append(
            Upgrade(
                _upgrade.upgrade_id,
                ability.ability_id,
                _upgrade.name,
                _upgrade.mineral_cost,
                _upgrade.vespene_cost,
                _upgrade.research_time,
                ability.button_name,
                ability.friendly_name.replace("Research", "")
                .replace("Evolve", "")
                .strip(),
                pysc2_name,
            )
        )
    return res


def parse_units(valid_units):
    """"""
    units: list[Unit] = []
    for unit in valid_units:
        units.append(
            Unit(
                unit.unit_id,
                unit.name,
                unit.mineral_cost,
                unit.vespene_cost,
                unit.build_time,
                unit.food_required,
            )
        )
    return units


def get_game_info(version: str) -> GameInfo:
    """Return dataclass that contains game information"""
    game = SC2Linux(version=version)
    create = make_creation_msg(game)
    interface = make_interface_opts()
    join = sc_pb.RequestJoinGame(
        options=interface, race=sc2_env.Race["protoss"], player_name="Mike Hunt"
    )

    vinfo = game.version
    game_info = GameInfo(f"{vinfo.game_version}.{vinfo.build_version}")
    with game.start(want_rgb=False, full_screen=False) as controller:
        controller.create_game(create)
        controller.join_game(join)
        game_data = controller.data()
        game_info.units = parse_units(
            (
                u
                for u in game_data.unit_stats.values()
                if u.name != "" and 0 < u.race < 4 and u.available and u.build_time > 0
            )
        )

        # game_info.upgrades = parse_upgrades(
        #     (u for u in game_data.upgrades.values() if u.ability_id != 0),
        #     game_data.abilities,
        # )
        def name_filter(a: AbilityData):
            return any(a.friendly_name.startswith(s) for s in ["Research", "Evolve"])

        game_info.upgrades = parse_upgrades2(
            (a for a in game_data.abilities.values() if name_filter(a)),
            game_data.upgrades,
        )
        controller.quit()

    return game_info


FLAGS = flags.FLAGS
flags.DEFINE_string("root", "", "")
flags.DEFINE_string("output", "", "")
flags.DEFINE_spaceseplist("versions", [""], "")


def main(unused_argv):
    """
    Launch each version of the game and inquire information about properties of each unit
    """
    root = Path(FLAGS.root)
    output = Path(FLAGS.output)

    assert output.parent.exists(), f"Output directory doesn't exist: {output.parent}"
    assert output.suffix == ".yaml", "Output must be a yaml file"

    versions = FLAGS.versions if FLAGS.versions else [p.name for p in root.iterdir()]

    game_infos: list[GameInfo] = []
    for idx, version in enumerate(versions, 1):
        print(f"CONVERTING {version}")
        os.environ["SC2PATH"] = str(root / version)
        game_infos.append(get_game_info(version))
        print(f"Finished {idx} of {len(versions)} game versions")

    with open(output, "w", encoding="utf-8") as f:
        yaml.dump([asdict(g) for g in game_infos], f)


if __name__ == "__main__":
    app.run(main)
