#!/usr/bin/env python3
"""Merge game info yaml files without duplication"""
from pathlib import Path

import typer
import yaml

app = typer.Typer()


@app.command()
def main(source: Path, dest: Path):
    """Copies info from source yaml to destination yaml without duplication"""
    with open(source, "r", encoding="utf-8") as src:
        source_data: list = yaml.safe_load(src)

    with open(dest, "r", encoding="utf-8") as dst:
        dest_data: list = yaml.safe_load(dst)

    existing = {entry["version"] for entry in dest_data}
    for entry in source_data:
        src_version = entry["version"]
        if src_version in existing:
            print(f"Skipping existing version {src_version}")
            continue
        print(f"Adding new version {src_version}")
        dest_data.append(entry)
        existing.add(src_version)

    with open(dest, "w", encoding="utf-8") as dst:
        yaml.safe_dump(dest_data, dst)


if __name__ == "__main__":
    app()
