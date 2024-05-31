from pathlib import Path
from typing import Annotated

import typer

from utils.replay_version import get_replay_file_version_info

app = typer.Typer()


def get_versions_in_tree(root: Path):
    """
    Recursively query all the sc2 replays in a directory and return a set of all versions present
    Also returns a mapping from unique versions to a replay with that version.
    """
    version_file_map: dict[tuple, Path] = {}

    replay_files = list(root.rglob("*SC2Replay"))
    for idx, file in enumerate(replay_files):
        try:
            version_info = get_replay_file_version_info(file)
            if any(v is None for v in version_info):
                version_info = None
        except (KeyError, ValueError):
            version_info = None

        if version_info is not None:
            version_file_map[version_info] = file

        print(f"Done {idx} of {len(replay_files)}", end="\r")

    return version_file_map


@app.command()
def compare_replays_and_game(
    replays: Annotated[Path, typer.Option()], game: Annotated[Path, typer.Option()]
):
    """Compare versions of replays with versions of the game verision currently present"""
    assert game.name == "Versions", f"Should point to the Versions folder, got {game}"
    current_bases = {folder.name[len("base") :] for folder in game.glob("Base*")}
    print("Game Build Versions: \n", current_bases)
    version_file_map = get_versions_in_tree(replays)

    missing_versions = {
        k: v for k, v in version_file_map.items() if k[2] not in current_bases
    }

    def sort_key(x: tuple[str]):
        """Split and accumulate game version"""
        a, b, c = x[0].split(".")
        return int(a) * 1e4 + int(b) * 1e2 + int(c)

    print(
        "Missing Build Versions: \n"
        + "\n".join(map(str, sorted(missing_versions, key=sort_key)))
    )
    print(
        "All Game Versions: \n"
        + "\n".join(map(str, sorted(version_file_map.keys(), key=sort_key)))
    )


@app.command()
def write_replay_versions(
    replays: Annotated[Path, typer.Option()],
    output: Annotated[Path, typer.Option()],
):
    """Write all the game,data,build versions found in folder of replays to a csv"""
    assert output.suffix == ".csv", f"Output should be a .csv file, got {output.suffix}"
    version_file_map = get_versions_in_tree(replays)

    with open(output, "w", encoding="utf-8") as out:
        out.write("game,data,build\n")
        for version in version_file_map:
            out.write(f"{','.join(version)}\n")


if __name__ == "__main__":
    app()
