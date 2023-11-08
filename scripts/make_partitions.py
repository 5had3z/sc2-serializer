#!/usr/bin/env python3
""" 
Script that creates the partition files
for running conversions in parallel.
"""
from pathlib import Path
from dataclasses import dataclass, field
from typing_extensions import Annotated

import typer

app = typer.Typer()


@dataclass
class ReplayFile:
    path: Path
    size: int


@dataclass
class Partition:
    files: list[ReplayFile] = field(default_factory=list)
    size: int = 0

    def append(self, file: ReplayFile):
        self.files.append(file)
        self.size += file.size


@app.command()
def main(
    folder: Annotated[Path, typer.Option(help="Path to folder of .SC2Replay files")],
    output: Annotated[Path, typer.Option(help="Folder to write the partition files")],
    num: Annotated[int, typer.Option(help="Number of partitions to generate")],
):
    """
    Read folder or SC2 Replay files and generate a set of files that
    contain an even size distribution of SC2 replay filenames
    """
    if not folder.exists():
        raise FileNotFoundError(f".SC2Replay folder not found: {folder}")
    if not output.exists():
        output.mkdir(parents=True)

    all_files = list(
        ReplayFile(f, f.stat().st_size)
        for f in folder.iterdir()
        if f.suffix == ".SC2Replay"
    )
    print(f"Found {len(all_files)} files")
    all_files.sort(key=lambda x: x.size, reverse=True)

    parts = [Partition() for _ in range(num)]
    for file in all_files:
        part = min(parts, key=lambda x: x.size)
        part.append(file)

    for i, part in enumerate(parts):
        outpath = output / f"partition_{i}"
        with open(outpath, "w", encoding="utf-8") as f:
            f.writelines(f.path.name + "\n" for f in part.files)


if __name__ == "__main__":
    app()
