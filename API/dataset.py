import torch
from torch.utils.data import Dataset
import urllib
import os
from pathlib import Path
from sc2_replay_reader import GAME_INFO_FILE, ReplayDatabase, ReplayParser, Result
import sqlite3
from scripts.utils import find_closest_indicies
from typing import List
import numpy as np
from dataclasses import dataclass


@dataclass
class TimeRange:
    min: float
    max: float
    step: float

    def __post_init__(self):
        assert self.min < self.max

    def arange(self):
        return torch.arange(self.min, self.max, self.step)


class SC2Dataset(Dataset):
    def __init__(
        self,
        basepath,
        download=True,
        transform=None,
        features: set[str] | None = None,
        database: Path = Path("sc2_dataset.db"),
        timepoints: TimeRange = TimeRange(0, 30, 0.5),
        sql_filters: List[str] | None = None,
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

        self.db_handle = ReplayDatabase()
        self.parser = ReplayParser(GAME_INFO_FILE)

        # Download the dataset if needed
        if download:
            self.download_database()
            self.download_data()

        self.load_database(database, self.sql_filters)
        self.load_files(basepath)

        self.sql_filters = sql_filters

        _loop_per_min = 22.4 * 60
        self._target_game_loops = (timepoints.arange() * _loop_per_min).to(torch.int)

    def load_database(self, database: Path, sql_filters: List[str] | None = None):
        self.database = database
        self.sql_filters = sql_filters
        sql_filter_string = (
            ""
            if self.sql_filters is None or len(self.sql_filters) == 0
            else (" " + " AND ".join(self.sql_filters))
        )
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

        sample_indicies = find_closest_indicies(
            self.parser.data.gameStep, self._target_game_loops[1:]
        )
        for idx in sample_indicies:
            if idx == -1:
                sample = {k: np.zeros_like(outputs_list[k][-1]) for k in outputs_list}
            else:
                sample = self.parser.sample(int(idx.item()))
            for k in outputs_list:
                outputs_list[k].append(sample[k])

        outputs = {
            "win": torch.as_tensor(
                self.parser.data.playerResult == Result.Win, dtype=torch.float32
            ),
            "valid": torch.cat([torch.tensor([True]), sample_indicies != -1]),
        }
        for k in outputs_list:
            outputs[k] = torch.stack([torch.as_tensor(o) for o in outputs_list[k]])

        return outputs

    def download_database(self):
        pass

    def download_data(self):
        # Create the root directory if it doesn't exist
        os.makedirs(self.root_dir, exist_ok=True)

        # Download the SC2 dataset
        dataset_url = (
            "https://example.com/sc2_dataset.zip"  # Replace with the actual URL
        )
        zip_file_path = os.path.join(self.root_dir, "sc2_dataset.zip")
        urllib.request.urlretrieve(dataset_url, zip_file_path)

        # Extract the downloaded zip file
        import zipfile

        with zipfile.ZipFile(zip_file_path, "r") as zip_ref:
            zip_ref.extractall(self.root_dir)

        # Remove the zip file after extraction
        os.remove(zip_file_path)
