from pathlib import Path
from typing import Callable, Dict, Literal, Tuple

import torch
from torch.utils.data import Dataset
from utils import upper_bound
from sc2_replay_reader import (
    GAME_INFO_FILE,
    ReplayDataAllDatabase,
    ReplayDataAllParser,
    setReplayDBLoggingLevel,
    spdlog_lvl,
)

SQL_TYPES = Literal["INTEGER", "FLOAT", "TEXT", "BOOLEAN"]
ENUM_KEYS = {"playerRace", "playerResult"}
LambdaFunctionType = Callable[[ReplayDataAllParser], float | int]


class SC2Replay(Dataset):
    def __init__(
        self,
        basepath: Path,
        features: set[str],
        lambda_columns: Dict[str, Tuple[SQL_TYPES, LambdaFunctionType]],
    ) -> None:
        super().__init__()
        self.features = features
        self.db_handle = ReplayDataAllDatabase()
        self.parser = ReplayDataAllParser(GAME_INFO_FILE)
        self.lambda_columns = lambda_columns

        setReplayDBLoggingLevel(spdlog_lvl.warn)

        if basepath.is_file():
            self.replays = [basepath]
        else:
            self.replays = list(basepath.glob("*.SC2Replays"))
            assert len(self.replays) > 0, f"No .SC2Replays found in {basepath}"

        replays_per_file = torch.empty([len(self.replays) + 1], dtype=torch.int)
        replays_per_file[0] = 0
        for idx, replay in enumerate(self.replays, start=1):
            self.db_handle.open(replay)
            replays_per_file[idx] = self.db_handle.size()

        self._accumulated_replays = torch.cumsum(replays_per_file, 0)
        self.n_replays = int(self._accumulated_replays[-1].item())
        assert self.n_replays > 0, "No replays in dataset"

    def __len__(self) -> int:
        return self.n_replays

    # @profile
    def __getitem__(self, index: int):
        file_index = upper_bound(self._accumulated_replays, index)
        self.db_handle.open(self.replays[file_index])
        db_index = index - int(self._accumulated_replays[file_index].item())
        assert (  # This should hold if calculation checks out
            db_index < self.db_handle.size()
        ), f"{db_index} exceeds {self.db_handle.size()}"

        try:
            self.parser.parse_replay(self.db_handle.getEntry(db_index))
        except MemoryError:
            data = {
                "partition": str(self.replays[file_index].name),
                "idx": db_index,
                "read_success": False,
            }
            try:
                hash, id = self.db_handle.getHashIdEntry(db_index)
                data["playerId"] = id
                data["replayHash"] = hash
            except MemoryError:
                pass

            return data

        data = {p: getattr(self.parser.info, p, None) for p in self.features}
        data = {k: int(v) if k in ENUM_KEYS else v for k, v in data.items()}

        for k, (_, f) in self.lambda_columns.items():
            data[k] = f(self.parser)

        return {
            "partition": str(self.replays[file_index].name),
            "idx": db_index,
            "read_success": True,
            **data,
        }

    def __iter__(self):
        for index in range(len(self)):
            yield self[index]
