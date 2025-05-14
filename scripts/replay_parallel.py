#!/usr/bin/env python3
"""Convert folders of replays in parallel"""

import concurrent.futures as fut
import io
import subprocess
from multiprocessing import cpu_count
from pathlib import Path
from typing import Annotated, Any

import typer

app = typer.Typer()


def make_args(
    game: Path, replays: Path, converter: Path, output: Path, port: int
) -> list[str]:
    """Convert arg list to kwargs"""
    return [
        str(converter.absolute()),
        f"--replays={replays}",
        f"--output={output}",
        f"--badfile={output.with_suffix('.bad.txt')}",
        "--converter=strided",
        f"--game={game}",
        f"--port={port}",
        "--save-actions",
        "--stride=22",
    ]


def run_with_redirect(tid: int, *args: Any) -> None:
    """Redirect each thread stdout to log file"""
    print(f"running {tid}: {args}")
    with open(f"worker_logs_{tid}.txt", "a", encoding="utf-8") as f:
        subprocess.run(
            args, stdout=io.TextIOWrapper(f.buffer, write_through=True), check=True
        )


def get_target_folders(base_folder: Path, replay_list: Path | None) -> list[Path]:
    """Get list of folders to run over"""
    if replay_list is not None:
        with open(replay_list, encoding="utf-8") as f:
            folders = f.readlines()
        targets = [base_folder / folder.strip() for folder in folders]
        for target in targets:
            if not target.exists():
                raise FileNotFoundError(target)
    else:
        targets = [folder for folder in base_folder.iterdir() if folder.is_dir()]
    return targets


@app.command()
def main(
    converter: Annotated[Path, typer.Option()],
    outfolder: Annotated[Path, typer.Option()],
    replays: Annotated[Path, typer.Option()],
    game: Annotated[Path, typer.Option()],
    replay_list: Annotated[Path | None, typer.Option()] = None,
    n_parallel: Annotated[int, typer.Option()] = cpu_count() // 2,
) -> None:
    """Convert replays at a target folder with n_parallel processes"""
    assert converter.exists()
    assert game.exists()
    assert replays.exists()
    if not 0 < n_parallel <= cpu_count():
        raise RuntimeError(
            f"{n_parallel=}, does not satisfy 0 < n_parallel < cpu_count()"
        )
    outfolder.mkdir(exist_ok=True)

    targets = get_target_folders(replays, replay_list)

    with fut.ProcessPoolExecutor(max_workers=n_parallel) as ctx:
        res: list[fut.Future] = []
        for idx, folder in enumerate(targets):
            outfile = outfolder / (folder.stem + ".SC2Replays")
            args = make_args(game, folder, converter, outfile, 9168 + idx)
            res.append(ctx.submit(run_with_redirect, idx, *args))
        fut.wait(res)


if __name__ == "__main__":
    app()
