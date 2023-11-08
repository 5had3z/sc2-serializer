#!/usr/bin/env python3
"""
Use PySC2 to generate C++ LUT
"""
from pathlib import Path

from pysc2.lib.units import Neutral, Protoss, Terran, Zerg
import typer

app = typer.Typer()


@app.command()
def main(out_folder: Path = Path.cwd()):
    content = r"""#pragma once
#include <unordered_set>

namespace cvt {
// clang-format off"""

    for race in [Neutral, Protoss, Terran, Zerg]:
        content += f"\nconst static std::unordered_set<int> {race.__name__.lower()}UnitTypes = {{ {','.join([str(e.value) for e in race])} }};\n"
    content += "// clang-format on\n}// namespace cvt\n"

    out_path = out_folder / "unit_ids.hpp"
    with open(out_path, "w", encoding="utf-8") as f:
        f.write(content)


if __name__ == "__main__":
    app()
