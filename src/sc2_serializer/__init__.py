"""Main Python Interface to SC2 Replay Data"""

from pathlib import Path

from ._sc2_serializer import (
    ReplayDataAllDatabase,
    ReplayDataNoUnitsDatabase,
    ReplayDataScalarOnlyDatabase,
    ReplayDataAllParser,
    ReplayDataNoUnitsParser,
    ReplayDataScalarOnlyParser,
)
from ._sc2_serializer import *  # noqa : F403

GAME_INFO_FILE = (Path(__file__).parent / "game_info.yaml").absolute()

INCLUDE_DIRECTORY = (Path(__file__).parent / "include").absolute()

ReplayDatabase = (
    ReplayDataAllDatabase | ReplayDataNoUnitsDatabase | ReplayDataScalarOnlyDatabase
)

ReplayParser = (
    ReplayDataAllParser | ReplayDataNoUnitsParser | ReplayDataScalarOnlyParser
)


def get_database_and_parser(
    parse_units: bool, parse_minimaps: bool, parser_info: Path = GAME_INFO_FILE
) -> tuple[ReplayDatabase, ReplayParser]:
    """
    Get database and parser pair with correct type depending on unit and minimap request.
    Due to the fundamental data structure, if parse_units is true, minimap data will also
    be included.
    """
    if parse_units:
        return ReplayDataAllDatabase(), ReplayDataAllParser(parser_info)
    if parse_minimaps:
        return ReplayDataNoUnitsDatabase(), ReplayDataNoUnitsParser(parser_info)
    return ReplayDataScalarOnlyDatabase(), ReplayDataScalarOnlyParser(parser_info)
