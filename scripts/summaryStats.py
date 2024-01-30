from pathlib import Path
from typing import Callable, Literal

import torch
from torch import Tensor
from torch.utils.data import Dataset

from sc2_replay_reader import (
    GAME_INFO_FILE,
    ReplayDataAllDatabase,
    ReplayDataAllParser,
    set_replay_database_logger_level,
    spdlog_lvl,
)

SQL_TYPES = Literal["INTEGER", "FLOAT", "TEXT", "BOOLEAN"]
ENUM_KEYS = {"playerRace", "playerResult"}
LambdaFunctionType = Callable[[ReplayDataAllParser], float | int]


def upper_bound(x: Tensor, value: float) -> int:
    """
    Find the index of the last element which is less or equal to value
    """
    return int(torch.argwhere(torch.le(x, value))[-1].item())


class SC2Replay(Dataset):
    def __init__(
        self,
        basepath: Path,
        features: set[str],
        lambda_columns: dict[str, tuple[SQL_TYPES, LambdaFunctionType]],
    ) -> None:
        super().__init__()
        self.features = features
        self.db_handle = ReplayDataAllDatabase()
        self.parser = ReplayDataAllParser(GAME_INFO_FILE)
        self.lambda_columns = lambda_columns

        set_replay_database_logger_level(spdlog_lvl.warn)

        if basepath.is_file():
            self.replays = [basepath]
        else:
            self.replays = list(basepath.glob("*.SC2Replays"))
            assert len(self.replays) > 0, f"No .SC2Replays found in {basepath}"

        replays_per_file = torch.empty([len(self.replays) + 1], dtype=torch.int)
        replays_per_file[0] = 0
        for idx, replay in enumerate(self.replays, start=1):
            if not self.db_handle.load(replay):
                raise FileNotFoundError
            replays_per_file[idx] = self.db_handle.size()

        self._accumulated_replays = torch.cumsum(replays_per_file, 0)
        self.n_replays = int(self._accumulated_replays[-1].item())
        assert self.n_replays > 0, "No replays in dataset"

    def __len__(self) -> int:
        return self.n_replays

    # @profile
    def __getitem__(self, index: int):
        file_index = upper_bound(self._accumulated_replays, index)
        if not self.db_handle.load(self.replays[file_index]):
            raise FileNotFoundError
        db_index = index - int(self._accumulated_replays[file_index].item())

        try:
            replay_data = self.db_handle.getEntry(db_index)
        except MemoryError:
            data = {
                "partition": str(self.replays[file_index].name),
                "idx": db_index,
                "read_success": False,
                "parse_success": False,
            }
            try:
                hash_, id_ = self.db_handle.getHashIdEntry(db_index)
                data["playerId"] = id_
                data["replayHash"] = hash_
            except MemoryError:
                pass

            return data

        try:
            self.parser.parse_replay(replay_data)
        except (IndexError, RuntimeError):
            parse_success = False
        else:
            parse_success = True

        data = {p: getattr(self.parser.info, p, None) for p in self.features}
        data = {k: int(v) if k in ENUM_KEYS else v for k, v in data.items()}

        for k, (_, f) in self.lambda_columns.items():
            data[k] = f(self.parser)

        return {
            "partition": str(self.replays[file_index].name),
            "idx": db_index,
            "read_success": True,
            "parse_success": parse_success,
            **data,
        }

    def __iter__(self):
        for index in range(len(self)):
            yield self[index]
