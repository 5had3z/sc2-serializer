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


@app.command()
def main(file: Annotated[Path, typer.Option()]):
    """"""
    if not file.exists():
        raise FileNotFoundError(file)

    resources: list[Resource] = []
    with open(file, "r", encoding="utf-8") as f:
        while line := f.readline():
            resources.append(Resource.from_line(line))

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
