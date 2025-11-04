from pathlib import Path
from tempfile import TemporaryDirectory
from typing import Annotated, Optional

import rich
import typer
from rich.progress import track

from sc2_serializer import ReplayDataAll, ReplayDataAllDatabase, StepDataSoA


def filesize_mb(path: Path):
    """Return the size of the file in megabytes"""
    if not path.exists():
        raise FileNotFoundError(path)
    return path.stat().st_size / (1024 * 1024)


def make_subslices(step_data: StepDataSoA, stride: int) -> dict[str, list]:
    """Make subslices of step_data with stride"""
    subslices = {}
    for key in dir(step_data):
        if key.startswith("__"):
            continue
        attr_data = getattr(step_data, key)
        subslices[key] = [
            attr_data[i : i + stride] for i in range(0, len(attr_data), stride)
        ]
    return subslices


def assign_subslice(
    step_data: StepDataSoA, subslices: dict[str, list], index: int
) -> None:
    """Assign each of the step_data attributes from the subslice dictionary index"""
    for attr, slice_data in subslices.items():
        setattr(step_data, attr, slice_data[index])


def evaluate_stride_database_size(
    database: ReplayDataAllDatabase, stride: int
) -> float:
    """Evaluate the size of the database created if each replay was chunked with stride."""
    num_replays = len(database)
    with TemporaryDirectory() as temp_dir:
        temp_file = Path(temp_dir) / "temp_db.SC2Replays"
        temp_db = ReplayDataAllDatabase(temp_file)

        for replay_idx in track(
            range(num_replays), total=num_replays, description=f"Processing {stride=}"
        ):
            entry = database[replay_idx]
            subslices = make_subslices(entry.data, stride)
            num_chunks = len(next(iter(subslices.values())))
            for chunk_idx in range(num_chunks):
                new_entry = ReplayDataAll()
                new_entry.header = entry.header
                new_entry.data = StepDataSoA()
                assign_subslice(new_entry.data, subslices, chunk_idx)
                temp_db.addEntry(new_entry)

        new_size = filesize_mb(temp_file)

    return new_size


def record_result(outpath: Path, stride: int, filesize: float):
    """Record the stride and filesize to the file"""
    with open(outpath, "a", encoding="utf-8") as f:
        f.write(f"{stride},{filesize}\n")


app = typer.Typer()


@app.command()
def main(
    sizes: list[int],
    replay_file: Annotated[Path, typer.Option()],
    out: Annotated[Optional[Path], typer.Option()],
):
    """Evaluate the filesize of a replay database if it were written with different chunk sizes
    for improved random access or subslicing of the replay"""
    rich.print(f"Original database size {filesize_mb(replay_file)} MB")
    if out is not None:
        with open(out, "w", encoding="utf-8") as f:
            f.write("stride,size_mb\n")

    database = ReplayDataAllDatabase(replay_file)
    for chunk_size in sizes:
        filesize = evaluate_stride_database_size(database, chunk_size)
        rich.print(f"Database is {filesize:.3f} MB with stride {chunk_size}")
        if out is not None:
            record_result(out, chunk_size, filesize)


if __name__ == "__main__":
    app()
