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

import typer
from pysc2.lib.units import Neutral, Protoss, Terran, Zerg

from utils import upgrade_map

app = typer.Typer()


def make_default_resources():
    """Create mapping between resource id and default quantity based on the resource name"""
    resource_mapping: dict[int, int] = {}
    for elem in Neutral:
        if "mineral" in elem.name.lower():
            resource_mapping[elem.value] = 900 if "750" in elem.name else 1800
        elif "vespene" in elem.name.lower():
            resource_mapping[elem.value] = 2250
    return resource_mapping


def add_unit_mapping(content: str):
    """Add sets to check belongings to groups"""
    content += "\n// Sets to check that a typeid belongs to a race\n"
    for race in [Neutral, Protoss, Terran, Zerg]:
        content += (
            f"const static std::unordered_set<int> {race.__name__.lower()}"
            f"UnitTypes = {{ {', '.join([str(e.value) for e in race])} }};\n"
        )
    return content


def add_resource_mapping(content: str):
    """Add mapping from type id to default resource quantity"""
    content += "\n// Default vespene or minerals from each resource type id\n"
    content += "const static std::unordered_map<int, int> defaultResources = { "
    content += ", ".join([f"{{{t},{q}}}" for t, q in make_default_resources().items()])
    content += " };\n"
    return content


def add_research_grouping(content: str):
    """Add set to group research by race"""
    content += "\n// Research for Each Race by Game Version\n"
    content += (
        "const static std::unordered_map<std::string, "
        "std::unordered_map<Race, std::set<int>>> raceResearch = {\n"
    )
    for version, data in upgrade_map.UPGRADE_INFO.items():
        content += f'    {{"{version}",\n        {{\n'
        for race in ["protoss", "terran", "zerg"]:
            content += f"            {{ Race::{race.capitalize()}, {{ "
            ids = sorted(getattr(data, race).keys())
            content += ", ".join([str(_id) for _id in ids]) + " } },\n"
        content += "        }\n    },\n"
    content += "};\n"
    return content


def add_research_remapping(content: str):
    """Add reserarch action remapping from non-leveled to leveled action"""
    content += "\n// Remap non-leveled research action to leveled research action\n"
    content += (
        "const static std::unordered_map<std::string, std::unordered_map<Race, "
        "std::unordered_map<int, std::array<int, 3>>>> raceResearchReID = {\n"
    )
    for version, data in upgrade_map.UPGRADE_INFO.items():
        content += f'    {{"{version}",\n        {{\n'
        for race in ["protoss", "terran", "zerg"]:
            content += f"            {{ Race::{race.capitalize()}, {{ "
            remappings = []
            for action, levels in getattr(data, f"{race}_lvl_remap").items():
                remappings.append(
                    f"{{ {action}, {{{', '.join([str(l) for l in levels])}}} }}"
                )
            content += ", ".join(remappings)
            content += " } },\n"
        content += "        }\n    },\n"
    content += "};\n"
    return content


@app.command()
def main(out_folder: Path = Path.cwd()):
    """Generate C++ Game Info Header based on values from PySC2"""
    content = r"""// ---- Generated by scripts/gen_info_header.py ----
#pragma once
#include "enums.hpp"

#include <array>
#include <set>
#include <unordered_map>
#include <unordered_set>

namespace cvt {
// clang-format off
"""
    content = add_unit_mapping(content)
    content = add_resource_mapping(content)
    content = add_research_grouping(content)
    content = add_research_remapping(content)

    # Footer
    content += "\n// clang-format on\n}// namespace cvt\n"

    # Write file
    out_path = out_folder / "generated_info.hpp"
    with open(out_path, "w", encoding="utf-8") as f:
        f.write(content)


if __name__ == "__main__":
    app()
