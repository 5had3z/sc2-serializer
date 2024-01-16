from pathlib import Path

try:
    from ._sc2_replay_reader import *  # noqa : F403
except ImportError:
    print(f"{__name__}: Unable to load C++ bindings")

GAME_INFO_FILE = (Path(__file__).parent / "game_info.yaml").absolute()
