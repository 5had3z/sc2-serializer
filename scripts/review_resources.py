#!/usr/bin/env python3
"""
Analyse how the resources in the map change over time.
"""

from dataclasses import dataclass
from pathlib import Path
from typing import Annotated

import numpy as np
import typer
from matplotlib import pyplot as plt

import sc2_serializer

app = typer.Typer()


@dataclass
class Resource:
    gid: int
    pos: np.ndarray
    qty: np.ndarray

    @classmethod
    def from_line(cls, line: str) -> "Resource":
        elems = [e.strip() for e in line.split(",")]
        gid = int(elems[0])
        position = np.array([float(e) for e in elems[1:4]])
        qty = np.array([int(e) for e in elems[4:]])
        return cls(gid, position, qty)


def from_experiment(file: Path) -> list[Resource]:
    """Read file generated from bin/experiment.cpp"""
    resources: list[Resource] = []
    with open(file, encoding="utf-8") as f:
        while line := f.readline():
            resources.append(Resource.from_line(line))
    return resources


def from_database(file: Path, idx: int) -> list[Resource]:
    """Read from proper database"""
    temp: dict[int, list[int]] = {}
    db = sc2_serializer.ReplayDatabase(file)
    replay_data = db.getEntry(idx)

    for unit in replay_data.data.neutralUnits[0]:
        if unit.contents > 0:
            temp[unit.id] = []

    for time_step in replay_data.data.neutralUnits:
        for unit in time_step:
            if unit.id in temp:
                temp[unit.id].append(unit.contents)

    resources: list[Resource] = []
    for unit in replay_data.data.neutralUnits[-1]:
        if unit.contents > 0:
            pos = np.array([unit.pos.x, unit.pos.y, unit.pos.z])
            resources.append(Resource(unit.id, pos, np.array(temp[unit.id])))

    return resources


def find_later_additions(file: Path, idx: int) -> None:
    db = sc2_serializer.ReplayDatabase(file)
    replay_data = db.getEntry(idx)

    starter_resources: dict[int, Resource] = {
        u.id: Resource(u.id, u.pos, 0) for u in replay_data.data.neutralUnits[0]
    }
    new_resources: list[Resource] = []

    for timestep in replay_data.data.neutralUnits:
        for unit in timestep:
            if unit.id not in starter_resources:
                print(unit.id)
                new_resources.append(Resource(unit.id, unit.pos, 0))
                # starters = np.stack([u.pos for u in starter_resources.values()])
                # dist = np.linalg.norm(starters - unit.pos, axis=-1, ord=2)
                starter_resources[unit.id] = new_resources[-1]
    pos = np.stack([u.pos for u in starter_resources.values()])
    plt.scatter(pos[..., 0], pos[..., 1], c="blue", alpha=0.5)
    pos = np.stack([u.pos for u in new_resources])
    plt.scatter(pos[..., 0], pos[..., 1], c="red", alpha=0.5)
    plt.show()


@app.command()
def main(
    file: Annotated[Path, typer.Option(help=".txt or .SC2Replays file to read")],
    idx: Annotated[int, typer.Option(help="Index to sample from .SC2Replays")] = 0,
) -> None:
    """Plot how the resources are changing over time. Good to check our modifications
    with visibility and default values are working as expected"""
    if not file.exists():
        raise FileNotFoundError(file)

    find_later_additions(file, idx)
    # return

    match file.suffix:
        case ".txt":
            resources = from_experiment(file)
        case ".SC2Replays":
            resources = from_database(file, idx)
        case _:
            raise NotImplementedError(f"Can't read {file.suffix} file")

    assert sum([1 for r in resources if r.qty.sum() == 0]) == 0, "Uninitialized found"

    pos = np.array([r.pos[..., :2] for r in resources])
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
