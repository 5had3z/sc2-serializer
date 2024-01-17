import io
import json

import mpyq


def read_replay_data(replay_path: str):
    """Return the replay data given a path to the replay."""
    with open(replay_path, "rb") as f:
        return f.read()


def extract_replay_version(replay_data: bytes):
    """Get the game, data and build info from replay bytes"""
    replay_io = io.BytesIO()
    replay_io.write(replay_data)
    replay_io.seek(0)
    archive = mpyq.MPQArchive(replay_io).extract()
    metadata: dict = json.loads(archive[b"replay.gamemetadata.json"].decode("utf-8"))
    game_version = ".".join(metadata["GameVersion"].split(".")[:-1])
    data_version = metadata.get("DataVersion")  # Only in replays version 4.1+.
    build_version = str(metadata["BaseBuild"][4:])

    return game_version, data_version, build_version


def get_replay_file_version_info(replay_path: str):
    """Get the replay game, data and build version from a file"""
    return extract_replay_version(read_replay_data(replay_path))
