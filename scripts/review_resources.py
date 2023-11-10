#!/usr/bin/env python3
""" 
Analyse how the resources in the map change over time.
"""
from dataclasses import dataclass
from pathlib import Path

import numpy as np
import typer
from matplotlib import pyplot as plt
from typing_extensions import Annotated

import sc2_replay_reader

app = typer.Typer()


@dataclass
class Resource:
    gid: int
    pos: np.ndarray
    qty: np.ndarray

    @classmethod
    def from_line(cls, line: str):
        elems = [e.strip() for e in line.split(",")]
        gid = int(elems[0])
        position = np.array([float(e) for e in elems[1:4]])
        qty = np.array([int(e) for e in elems[4:]])
        return cls(gid, position, qty)


def from_experiment(file: Path):
    """Read file generated from bin/experiment.cpp"""
    resources: list[Resource] = []
    with open(file, "r", encoding="utf-8") as f:
        while line := f.readline():
            resources.append(Resource.from_line(line))
    return resources


def from_database(file: Path, idx: int):
    """Read from proper database"""
    temp: dict[int, list[int]] = {}
    db = sc2_replay_reader.ReplayDatabase(file)
    replay_data = db.getEntry(idx)

    for unit in replay_data.neutralUnits[0]:
        if unit.contents > 0:
            temp[unit.id] = []

    for time_step in replay_data.neutralUnits:
        for unit in time_step:
            if unit.id in temp:
                temp[unit.id].append(unit.contents)

    resources: list[Resource] = []
    for unit in replay_data.neutralUnits[0]:
        if unit.contents > 0:
            pos = np.array([unit.pos.x, unit.pos.y, unit.pos.z])
            resources.append(Resource(unit.id, pos, np.array(temp[unit.id])))

    return resources


@app.command()
def main(
    file: Annotated[Path, typer.Option(help=".txt or .SC2Replays file to read")],
    idx: Annotated[int, typer.Option(help="Index to sample from .SC2Replays")] = 0,
):
    """"""
    if not file.exists():
        raise FileNotFoundError(file)

    match file.suffix:
        case ".txt":
            resources = from_experiment(file)
        case ".SC2Replays":
            resources = from_database(file, idx)
        case _:
            raise NotImplementedError(f"Can't read {file.suffix} file")

    assert sum([1 for r in resources if r.qty.sum() == 0]) == 0, "Uninitialized found"

    pos = np.array([r.pos for r in resources])
    dist = np.linalg.norm(pos[None] - pos[:, None], axis=-1, ord=2)
    plt.scatter(pos[..., 0], pos[..., 1])
    plt.show()
    assert (dist == 0).sum() == len(resources), "Position duplicates"

    mined = [r for r in resources if r.qty[0] != r.qty[-1]]
    for m in mined:
        plt.plot(m.qty)
    plt.show()


if __name__ == "__main__":
    app()
