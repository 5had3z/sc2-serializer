import io
import json

import mpyq


def get_replay_data(replay_path: str):
    """Return the replay data given a path to the replay."""
    with open(replay_path, "rb") as f:
        return f.read()


def get_replay_version(replay_data: bytes):
    replay_io = io.BytesIO()
    replay_io.write(replay_data)
    replay_io.seek(0)
    archive = mpyq.MPQArchive(replay_io).extract()
    metadata = json.loads(archive[b"replay.gamemetadata.json"].decode("utf-8"))
    game_version = ".".join(metadata["GameVersion"].split(".")[:-1])
    data_version = metadata.get("DataVersion")  # Only in replays version 4.1+.
    build_version = str(metadata["BaseBuild"][4:])

    return game_version, data_version, build_version


def get_all_versions(replay_data: bytes):
    try:
        replay_io = io.BytesIO()
        replay_io.write(replay_data)
        replay_io.seek(0)
        archive = mpyq.MPQArchive(replay_io).extract()
        metadata = json.loads(archive[b"replay.gamemetadata.json"].decode("utf-8"))
        game_version = ".".join(metadata["GameVersion"].split(".")[:-1])
        data_version = metadata.get("DataVersion")  # Only in replays version 4.1+.
        build_version = int(metadata["BaseBuild"][4:])

        if any(k is None for k in [game_version, data_version, build_version]):
            return None

        return game_version, data_version, build_version
    except KeyError:
        return None
    except ValueError:
        return None


def run_file(file_path: str):
    return get_replay_version(get_replay_data(file_path))
