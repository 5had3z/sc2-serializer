from pathlib import Path

from ._sc2_replay_reader import *  # noqa : F403

GAME_INFO_FILE = (Path(__file__).parent / "game_info.yaml").absolute()
