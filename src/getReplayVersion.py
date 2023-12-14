import io
import json
import mpyq
import sys


def replay_data(replay_path: str):
    """Return the replay data given a path to the replay."""
    with open(replay_path, "rb") as f:
        return f.read()


def get_replay_version(replay_data):
    replay_io = io.BytesIO()
    replay_io.write(replay_data)
    replay_io.seek(0)
    archive = mpyq.MPQArchive(replay_io).extract()
    metadata = json.loads(archive[b"replay.gamemetadata.json"].decode("utf-8"))
    game_version=".".join(metadata["GameVersion"].split(".")[:-1])

    return (game_version, metadata.get("DataVersion"))  # Only in replays version 4.1+.


def run_file(file_path: str):
    return get_replay_version(replay_data(file_path))


if __name__ == "__main__":
    rd = replay_data(sys.argv[1])
    print(get_replay_version(rd))
