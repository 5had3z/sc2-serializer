import io
import json
import mpyq
from pathlib import Path
import typer
from typing_extensions import Annotated

app = typer.Typer()


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
    game_version = ".".join(metadata["GameVersion"].split(".")[:-1])
    data_version = metadata.get("DataVersion")  # Only in replays version 4.1+.
    build_version = str(metadata["BaseBuild"][4:])

    return game_version, data_version, build_version


def get_all_versions(replay_data):
    try:
        replay_io = io.BytesIO()
        replay_io.write(replay_data)
        replay_io.seek(0)
        archive = mpyq.MPQArchive(replay_io).extract()
        metadata = json.loads(archive[b"replay.gamemetadata.json"].decode("utf-8"))
        game_version = ".".join(metadata["GameVersion"].split(".")[:-1])
        data_version = metadata.get("DataVersion")  # Only in replays version 4.1+.
        build_version = int(metadata["BaseBuild"][4:])

        if game_version is None or data_version is None or build_version is None:
            return None

        return game_version, data_version, build_version
    except KeyError:
        return None
    except ValueError:
        return None


def run_file(file_path: str):
    return get_replay_version(replay_data(file_path))


@app.command()
def main(
    replay_paths: Annotated[Path, typer.Option()],
    game_paths: Annotated[Path, typer.Option()],
):
    all_versions = set()

    current_bases = set(game_paths.glob("*"))

    games = []
    missing_versions = []
    for i in replay_paths.rglob("*.SC2Replay"):
        versions = get_all_versions(replay_data(i))
        if versions is not None:
            if str(versions[2]) not in current_bases:
                missing_versions.append([i, *versions])
                current_bases.append(str(versions[2]))

            if versions not in all_versions:
                games.append([i, *versions])

            all_versions.add((versions))
    print(
        "Missing versions: \n"
        + "\n".join(map(str, sorted(missing_versions, key=lambda x: x[0])))
    )
    print("All versions: \n" + "\n".join(map(str, sorted(games, key=lambda x: x[0]))))


if __name__ == "__main__":
    app()
