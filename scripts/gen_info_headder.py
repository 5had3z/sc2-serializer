#!/usr/bin/env python3
"""
Use PySC2 to generate required C++ Data

For generating the default resource quantities, for minearals it
should be 1800 or 900 depending on if "750" is in the name, as
the default was changed to these two values since Legacy of the Void
Vespene is always 2250 since patch 4.0
https://starcraft.fandom.com/wiki/Vespene_gas
https://starcraft.fandom.com/wiki/Minerals
"""
from pathlib import Path

from pysc2.lib.units import Neutral, Protoss, Terran, Zerg
import typer

app = typer.Typer()


def make_default_resources():
    resource_mapping: dict[int, int] = {}
    for elem in Neutral:
        if "mineral" in elem.name.lower():
            resource_mapping[elem.value] = 900 if "750" in elem.name else 1800
        elif "vespene" in elem.name.lower():
            resource_mapping[elem.value] = 2250
    return resource_mapping


@app.command()
def main(out_folder: Path = Path.cwd()):
    content = r"""// ---- Generated by scripts/generate_enum.py ----
#pragma once
#include <unordered_map>
#include <unordered_set>

namespace cvt {
// clang-format off
"""

    # Add sets to check belongings to groups
    content += "\n// Sets to check that a typeid belongs to a race\n"
    for race in [Neutral, Protoss, Terran, Zerg]:
        content += (
            f"const static std::unordered_set<int> {race.__name__.lower()}"
            f"UnitTypes = {{ {', '.join([str(e.value) for e in race])} }};\n"
        )

    # Add mapping from type id to default resource quantity
    content += "\n// Default vespene or minerals from each resource type id\n"
    content += "const static std::unordered_map<int, int> defaultResources = {{ "
    content += ", ".join([f"{{{t},{q}}}" for t, q in make_default_resources().items()])
    content += " };\n"

    # Footer
    content += "\n// clang-format on\n}// namespace cvt\n"

    # Write file
    out_path = out_folder / "generated_info.hpp"
    with open(out_path, "w", encoding="utf-8") as f:
        f.write(content)


if __name__ == "__main__":
    app()