#!/usr/bin/env python3
"""
Explore SC2Replays data with python API
"""
# ruff: noqa
import time
from enum import Enum
from pathlib import Path
from typing import Annotated, Sequence

import cv2
import numpy as np
import typer
from matplotlib import pyplot as plt
from matplotlib.backends.backend_agg import FigureCanvasAgg

from sc2_replay_reader import (
    ReplayDataAllDatabase,
    ReplayDatabase,
    ReplayDataScalarOnlyDatabase,
    ReplayParser,
    get_database_and_parser,
    set_replay_database_logger_level,
    spdlog_lvl,
)
from sc2_replay_reader.unit_features import NeutralUnit, NeutralUnitOH, Unit, UnitOH

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
    start = time.time()
    parser.setMinimapFeatures(["player_relative", "visibility", "creep"])
    for idx in range(db.size()):
        replay_data = db.getEntry(idx)
        parser.parse_replay(replay_data)
        _res = parser.sample(100)
        print(f"Done {idx+1} of {db.size()}", end="\r")
    print(f"\nFinished parsing, took {time.time() - start}s")


@app.command()
def check_first_step(path: Path, threshold: int = 224):
    """Check the first recorded gameStep of a replay and warn when its over a threshold"""
    set_replay_database_logger_level(spdlog_lvl.warn)
    db = ReplayDataScalarOnlyDatabase()
    if path.is_file():
        replays = [path]
    else:
        replays = list(path.glob("*.SC2Replays"))

    count_bad = 0
    total_replays = 0
    with typer.progressbar(replays, length=len(replays)) as pbar:
        replay: Path
        for replay in pbar:
            if not db.load(replay):
                raise FileNotFoundError("???")
            for idx in range(db.size()):
                sample = db.getEntry(idx)
                first_step = sample.data.gameStep[0]
                if first_step > threshold:
                    replayHash = sample.header.replayHash
                    playerId = sample.header.playerId
                    filename = replay.stem
                    print(
                        f"high initial gamestep {first_step} in {replayHash=}, {playerId=}, {filename=}"
                    )
                    count_bad += 1
            total_replays += db.size()

    print(f"Final count of bad replays: {count_bad}")


@app.command()
def count(folder: Annotated[Path, typer.Option(help="Folder to count replays")]):
    """Count number of replays in a set of shards"""
    db = ReplayDataAllDatabase()
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
    db, parser = get_database_and_parser(parse_units=True, parse_minimaps=True)
    db.open(file)

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
