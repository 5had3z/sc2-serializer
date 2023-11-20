"""
Explore SC2Replays data with python API
"""
# ruff: noqa
from pathlib import Path
from typing import Sequence

import cv2
import numpy as np
import sc2_replay_reader
import typer
from typing_extensions import Annotated
from matplotlib import pyplot as plt

from sc2_replay_reader.unit_features import Unit, UnitOH, NeutralUnit, NeutralUnitOH

app = typer.Typer()


def make_minimap_video(image_sequence: Sequence, fname: Path):
    """Make video from image sequence"""
    image = image_sequence[0]
    writer = cv2.VideoWriter(
        str(fname), cv2.VideoWriter_fourcc(*"VP90"), 10, image.data.shape, isColor=False
    )
    assert writer.isOpened(), f"Failed to open {fname}"
    for image in image_sequence:
        writer.write(
            cv2.normalize(image.data, None, 0, 255, cv2.NORM_MINMAX, dtype=cv2.CV_8U)
        )
    writer.release()


def make_units_video(parser: sc2_replay_reader.ReplayParser, fname: Path):
    writer = cv2.VideoWriter(
        str(fname), cv2.VideoWriter_fourcc(*"VP90"), 10, (800, 800), isColor=False
    )
    for tidx in range(parser.size()):
        sample = parser.sample(tidx)

    writer.release()


def test_parse(db, idx):
    replay_data = db.getEntry(idx)
    parser = sc2_replay_reader.ReplayParser(sc2_replay_reader.GAME_INFO_FILE)
    parser.parse_replay(replay_data)


@app.command()
def main(
    file: Annotated[Path, typer.Option(help="SC2Replays file")],
    idx: Annotated[int, typer.Option(help="Replay Index")] = 0,
    outfolder: Annotated[
        Path, typer.Option(help="Directory to write data")
    ] = Path.cwd()
    / "workspace",
):
    """"""
    db = sc2_replay_reader.ReplayDatabase(file)
    # for i in range(db.size()):
    #     test_parse(db, i)

    parser = sc2_replay_reader.ReplayParser(sc2_replay_reader.GAME_INFO_FILE)
    parser.parse_replay(db.getEntry(idx))
    make_units_video(parser, outfolder / "raw_units.webm")

    # fmt: off
    img_attrs = [
        "alerts", "buildable", "creep", "pathable",
        "visibility", "player_relative",
    ]
    # fmt: on
    # for attr in img_attrs:
    #     image_sequence = getattr(replay_data, attr)
    #     make_video(image_sequence, outfolder / f"{attr}.webm")

    print("Done")


if __name__ == "__main__":
    app()
