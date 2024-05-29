import sqlite3
from dataclasses import dataclass
from enum import Enum
from pathlib import Path

import numpy as np
import torch
from download import download_file, split_and_download
from file_links import database as database_link
from file_links import links, tourn_links
from torch.utils.data import Dataset
from utils import find_closest_indices

from sc2_replay_reader import (
    GAME_INFO_FILE,
    ReplayDataAllDatabase,
    ReplayDataAllParser,
    Result,
)


@dataclass
class TimeRange:
    min: float
    max: float
    step: float

    def __post_init__(self):
        assert self.min < self.max

    def arange(self):
        return torch.arange(self.min, self.max, self.step)


class DatasetType(Enum):
    Tournament = 0
    ReplayPacks = 1
    Both = 2


class SC2Dataset(Dataset):
    def __init__(
        self,
        basepath: Path,
        download=True,
        dataset_size=1.0,
        dataset_type=DatasetType.ReplayPacks,
        dataset_folder=Path("./dataset"),
        transform=None,
        features: set[str] | None = None,
        database: Path = Path("sc2_dataset.db"),
        timepoints: TimeRange = TimeRange(0, 30, 0.5),
        sql_filters: list[str] | None = None,
    ):
        """
        Args:
            root_dir (string): Directory where the dataset will be stored.
            download (bool, optional): Whether to download the dataset.
            transform (callable, optional): Optional transform to be applied on a sample.
        """
        self.root_dir = basepath
        self.transform = transform
        self.features = features

        self.db_handle = ReplayDataAllDatabase()
        self.parser = ReplayDataAllParser(GAME_INFO_FILE)

        # Download the dataset if needed
        if download:
            self.files = []
            download_file(database_link)
            if dataset_type == DatasetType.ReplayPacks:
                # Assume homogeneous
                for i in range(int(round(len(links) * dataset_size))):
                    self.files.append(
                        download_file(links[i]), dataset_folder / "replay_packs"
                    )
            elif dataset_type == DatasetType.Tournament:
                self.files = split_and_download(
                    tourn_links, dataset_size, dataset_folder / "tournament"
                )
            else:
                self.files = split_and_download(
                    tourn_links, dataset_size, dataset_folder / "tournament"
                )
                self.files += split_and_download(
                    links, dataset_size, dataset_folder / "replay_packs"
                )

        else:
            # get files somehow??
            if dataset_type == DatasetType.ReplayPacks:
                files = (dataset_folder / "replay_packs").glob("*")
                self.files = self.files[: int(round(dataset_size * len(files)))]
            elif dataset_type == DatasetType.Tournament:
                files = (dataset_folder / "tournament").glob("*")
                self.files = self.files[: int(round(dataset_size * len(files)))]
            else:
                files1 = (dataset_folder / "replay_packs").glob("*")
                files1 = files1[: int(round(dataset_size * len(files1)))]

                files2 = (dataset_folder / "tournament").glob("*")
                files2 = files2[: int(round(dataset_size * len(files2)))]
                self.files = files1 + files2

        self.load_database(database, self.sql_filters)
        self.load_files(basepath)

        self.sql_filters = sql_filters

        _loop_per_min = 22.4 * 60
        self._target_game_loops = (timepoints.arange() * _loop_per_min).to(torch.int)

    def load_database(self, database: Path, sql_filters: list[str] | None = None):
        self.database = database
        self.sql_filters = sql_filters
        sql_filter_string = (
            ""
            if self.sql_filters is None or len(self.sql_filters) == 0
            else (" " + " AND ".join(self.sql_filters))
        )
        sql_filter_string += f"WHERE partition IN ({','.join(self.files)})"
        self.sql_query = "SELECT * FROM game_data" + sql_filter_string + ";"

        assert sqlite3.complete_statement(self.sql_query), "Incomplete SQL Statement"
        with sqlite3.connect(self.database) as conn:
            cursor = conn.cursor()
            try:
                cursor.execute(self.sql_query)
            except sqlite3.OperationalError as e:
                raise AssertionError("Invalid SQL Syntax", e) from e

        # Extract and print column names
        self.cursor.execute("PRAGMA table_info('game_data');")
        # Fetch all rows containing column information
        columns_info = self.cursor.fetchall()
        self.column_names = [column[1] for column in columns_info]
        self.file_name_idx = self.column_names.index("partition")
        self.idx_idx = self.column_names.index("idx")

    def __len__(self):
        return self.n_replays

    def load_files(self, basepath: Path):
        self.basepath = basepath
        self.conn = sqlite3.connect(self.database)
        self.cursor = self.conn.cursor()
        self.cursor.execute(self.sql_query.replace(" * ", " COUNT(*) "))

        self.n_replays = self.cursor.fetchone()[0]

    def __getitem__(self, index: int):
        squery = self.sql_query[:-1] + f" LIMIT 1 OFFSET {index};"
        self.cursor.execute(squery)
        result = self.cursor.fetchone()

        return self.getitem(
            self.basepath / result[self.file_name_idx], result[self.idx_idx]
        )

    def getitem(self, file_name: Path, db_index: int):
        self.db_handle.open(file_name)
        assert (  # This should hold if calculation checks out
            db_index < self.db_handle.size()
        ), f"{db_index} exceeds {self.db_handle.size()}"

        self.parser.parse_replay(self.db_handle.getEntry(db_index))

        outputs_list = self.parser.sample(0)
        if self.features is not None:
            outputs_list = {k: [outputs_list[k]] for k in self.features}
        else:
            outputs_list = {k: [outputs_list[k]] for k in outputs_list}

        sample_indices = find_closest_indices(
            self.parser.data.gameStep, self._target_game_loops[1:]
        )
        for idx in sample_indices:
            if idx == -1:
                sample = {k: np.zeros_like(outputs_list[k][-1]) for k in outputs_list}
            else:
                sample = self.parser.sample(int(idx.item()))
            for k in outputs_list:
                outputs_list[k].append(sample[k])

        outputs = {
            "win": torch.as_tensor(
                self.parser.info.playerResult == Result.Win, dtype=torch.float32
            ),
            "valid": torch.cat([torch.tensor([True]), sample_indices != -1]),
        }
        for k in outputs_list:
            outputs[k] = torch.stack([torch.as_tensor(o) for o in outputs_list[k]])

        return outputs


if __name__ == "__main__":
    d = SC2Dataset(Path("."), True, 0.5, DatasetType.Tournament)
