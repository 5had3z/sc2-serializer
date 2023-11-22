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
from matplotlib.backends.backend_agg import FigureCanvasAgg

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
        ax.set_xlim(0, parser.data.mapWidth)
        ax.set_ylim(0, parser.data.mapHeight)
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
        print(f"Done {tidx}/{parser.size()}", end="\r")
    print("")

    writer.release()


def test_parse(db, idx, parser):
    replay_data = db.getEntry(idx)
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
    parser = sc2_replay_reader.ReplayParser(sc2_replay_reader.GAME_INFO_FILE)
    # for i in range(db.size()):
    #     test_parse(db, i, parser)

    parser.parse_replay(db.getEntry(idx))
    make_units_video(parser, outfolder / "raw_units1.webm")

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
