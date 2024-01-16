#!/usr/bin/env python3
"""
Explore SC2Replays data with python API
"""
# ruff: noqa
from enum import Enum
from pathlib import Path
from typing import Sequence

import cv2
import numpy as np
import typer
from typing_extensions import Annotated
from matplotlib import pyplot as plt
from matplotlib.backends.backend_agg import FigureCanvasAgg

from sc2_replay_reader import ReplayDatabase, ReplayParser, GAME_INFO_FILE
from sc2_replay_reader.unit_features import Unit, UnitOH, NeutralUnit, NeutralUnitOH

app = typer.Typer()


def make_minimap_video(image_sequence: Sequence, fname: Path):
    """Make video from image sequence"""
    image = image_sequence[0]
    writer = cv2.VideoWriter(
        str(fname), cv2.VideoWriter_fourcc(*"VP90"), 10, image.shape, isColor=False
    )
    assert writer.isOpened(), f"Failed to open {fname}"
    for image in image_sequence:
        writer.write(
            cv2.normalize(image.data, None, 0, 255, cv2.NORM_MINMAX, dtype=cv2.CV_8U)
        )
    writer.release()


def make_units_video(parser: ReplayParser, fname: Path):
    img_w = 1920
    img_h = 1080
    writer = cv2.VideoWriter(
        str(fname), cv2.VideoWriter_fourcc(*"VP90"), 10, (img_w, img_h)
    )
    dpi = 200
    fig_w = img_w / dpi
    fig_h = img_h / dpi
    fig, ax = plt.subplots(figsize=(fig_w, fig_h), dpi=dpi)

    for tidx in range(parser.size()):
        sample = parser.sample(tidx)
        ax.clear()
        ax.set_xlim(0, parser.info.mapWidth)
        ax.set_ylim(0, parser.info.mapHeight)
        unit_xy = sample["units"][:, [UnitOH.x, UnitOH.y]]
        for a, c in zip([UnitOH.alliance_self, UnitOH.alliance_enemy], ["blue", "red"]):
            unit_filt = unit_xy[sample["units"][:, a] == 1]
            ax.scatter(unit_filt[:, 0], unit_filt[:, 1], c=c)
        canvas = FigureCanvasAgg(fig)
        canvas.draw()
        rgb = np.frombuffer(canvas.tostring_rgb(), dtype=np.uint8).reshape(
            canvas.get_width_height()[::-1] + (3,)
        )
        writer.write(cv2.cvtColor(rgb, cv2.COLOR_RGB2BGR))
        if not writer.isOpened():
            raise RuntimeError()
        print(f"Done {tidx+1}/{parser.size()}", end="\r")
    print("")

    writer.release()


def test_parseable(db: ReplayDatabase, parser: ReplayParser):
    """Try parse db at index with parser"""
    for idx in range(db.size()):
        replay_data = db.getEntry(idx)
        parser.parse_replay(replay_data)
        print(f"Done {idx+1} of {db.size()}", end="\r")
    print("\nOk")


@app.command()
def count(folder: Annotated[Path, typer.Option(help="Folder to count replays")]):
    """Count number of replays in a set of shards"""
    db = ReplayDatabase()
    total = 0
    for file in folder.iterdir():
        if file.suffix == ".SC2Replays":
            db.open(file)
            print(f"found {db.size()} replays in {file.name}")
            total += db.size()
    print(f"Total replays: {total}")


class SubCommand(str, Enum):
    scatter_units = "scatter_units"
    minimap_video = "minimap_video"
    test_parseable = "test_parseable"


@app.command()
def inspect(
    file: Annotated[Path, typer.Option(help="SC2Replays file")],
    command: Annotated[SubCommand, typer.Option(help="Thing to run on data")],
    idx: Annotated[int, typer.Option(help="Replay Index")] = 0,
    outfolder: Annotated[
        Path, typer.Option(help="Directory to write data")
    ] = Path.cwd()
    / "workspace",
):
    """"""
    db = ReplayDatabase(file)
    parser = ReplayParser(GAME_INFO_FILE)

    if command is SubCommand.test_parseable:
        test_parseable(db, parser)

    elif command is SubCommand.scatter_units:
        outfolder.mkdir(exist_ok=True)
        parser.parse_replay(db.getEntry(idx))
        make_units_video(parser, outfolder / "raw_units.webm")

    elif command is SubCommand.minimap_video:
        outfolder.mkdir(exist_ok=True)
        # fmt: off
        img_attrs = [
            "alerts", "buildable", "creep", "pathable",
            "visibility", "player_relative",
        ]
        # fmt: on
        replay_data = db.getEntry(idx)
        for attr in img_attrs:
            image_sequence = getattr(replay_data.data, attr)
            make_minimap_video(image_sequence, outfolder / f"{attr}.webm")


if __name__ == "__main__":
    app()
