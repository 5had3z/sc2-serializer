"""
Replay sampling classes for yielding replays when dataloading.
"""

import os
import sqlite3
from abc import ABC, abstractmethod
from contextlib import closing
from pathlib import Path

import numpy as np

from ._sc2_serializer import ReplayDataScalarOnlyDatabase


class ReplaySampler(ABC):
    """Abstract interface to sample from set of replay databases"""

    def __init__(self, train_ratio: float, is_train: bool) -> None:
        self.train_ratio = train_ratio
        self.is_train = is_train

    @abstractmethod
    def __len__(self) -> int:
        ...

    @abstractmethod
    def sample(self, index: int) -> tuple[Path, int]:
        """Sample 'index' from replays by calculating the file and index in that file to sample at.

        Returns:
            tuple[Path,int]: Filepath to database and index
        """

    def get_split_params(self, dataset_size: int):
        """Calculate start offset + size based on the full dataset size.

        Args:
            dataset_size (int): Size of the full dataset aka total number of replays

        Returns:
            tuple[int,int]: start_idx and n_replays for this split
        """
        train_size = int(dataset_size * self.train_ratio)

        if self.is_train:
            start_idx = train_size
            n_replays = dataset_size - train_size
        else:
            start_idx = 0
            n_replays = train_size

        assert n_replays > 0, f"n_replays is not greater than zero: {n_replays}"

        return start_idx, n_replays


class BasicSampler(ReplaySampler):
    """Sample replays from single file or folder of replay files.

    Args:
        path (Path): Path to individual or folder of .SC2Replays file(s)
        train_ratio (float): Proportion of all data used for training
        is_train (bool): whether to sample from train or test|val split

    Raises:
        FileNotFoundError: path doesn't exist
        AssertionError: no .SC2Replay files found in folder
    """

    def __init__(self, replays_path: Path, train_ratio: float, is_train: bool) -> None:
        super().__init__(train_ratio, is_train)
        if not replays_path.exists():
            raise FileNotFoundError(
                f"Replay dataset or folder doesn't exist: {replays_path}"
            )

        if replays_path.is_file():
            self.replays = [replays_path]
        else:
            self.replays = list(replays_path.glob("*.SC2Replays"))
            assert len(self.replays) > 0, f"No .SC2Replays found in {replays_path}"

        replays_per_file = np.empty([len(self.replays) + 1], dtype=int)
        replays_per_file[0] = 0

        db_interface = ReplayDataScalarOnlyDatabase()

        for idx, replay in enumerate(self.replays, start=1):
            db_interface.load(replay)
            replays_per_file[idx] = db_interface.size()

        self._accumulated_replays = np.cumsum(replays_per_file, 0)
        self.start_idx, self.n_replays = self.get_split_params(
            int(self._accumulated_replays[-1].item())
        )

    def __len__(self) -> int:
        return self.n_replays

    def sample(self, index: int) -> tuple[Path, int]:
        file_index = upper_bound(self._accumulated_replays, self.start_idx + index)
        db_index = index - int(self._accumulated_replays[file_index].item())
        return self.replays[file_index], db_index


class SQLSampler(ReplaySampler):
    """Use SQLite3 database to yield from a folder of replay databases with filters applied.

    Args:
        database (str): Path to sqlite3 database with replay info, prefix '$ENV:' will be prefixed with DATAPATH.
        replays_folder (Path): Path to folder of .SC2Replays file(s)
        filter_query (str): SQL query to filter sampled replays
        train_ratio (float): Proportion of all data used for training
        is_train (bool): whether to sample from train or test|val split

    Raises:
        AssertionError: replays_folder doesn't exist
        AssertionError: no .SC2Replay files found in folder
    """

    def __init__(
        self,
        database: str,
        replays_path: Path,
        filter_query: str | list[str],
        train_ratio: float,
        is_train: bool,
    ) -> None:
        super().__init__(train_ratio, is_train)
        if database.startswith("$ENV:"):
            self.database_path = Path(
                os.environ.get("DATAPATH", "/data")
            ) / database.removeprefix("$ENV:")
        else:
            self.database_path = Path(database)

        if not self.database_path.is_file():
            raise FileNotFoundError(self.database_path)

        with closing(sqlite3.connect(self.database_path)) as db:
            if isinstance(filter_query, list):
                filter_query = gen_val_query(db, filter_query)
            self.filter_query = filter_query

            assert replays_path.exists(), f"replays_path not found: {replays_path}"
            self.replays_path = replays_path

            cursor = db.cursor()
            cursor.execute(filter_query.replace(" * ", " COUNT (*) "))
            self.start_idx, self.n_replays = self.get_split_params(cursor.fetchone()[0])

            cursor.execute("PRAGMA table_info('game_data');")
            column_names = [col_info[1] for col_info in cursor.fetchall()]

        self.filename_col: int = column_names.index("partition")
        self.index_col: int = column_names.index("idx")

    def __len__(self) -> int:
        return self.n_replays

    def sample(self, index: int) -> tuple[Path, int]:
        query = self.filter_query[:-1] + f" LIMIT 1 OFFSET {self.start_idx + index};"

        with closing(sqlite3.connect(self.database_path)) as db:
            cursor = db.cursor()
            cursor.execute(query)
            result = cursor.fetchone()

        return self.replays_path / result[self.filename_col], result[self.index_col]


def gen_val_query(conn: sqlite3.Connection, sql_filters: list[str] | None):
    """Transform list of sql filters to valid query and test that it works"""
    sql_filter_string = (
        ""
        if sql_filters is None or len(sql_filters) == 0
        else (" WHERE " + " AND ".join(sql_filters))
    )
    sql_query = "SELECT * FROM game_data" + sql_filter_string + ";"
    assert sqlite3.complete_statement(sql_query), "Incomplete SQL Statement"
    cursor = conn.cursor()
    try:
        cursor.execute(sql_query)
    except sqlite3.OperationalError as e:
        raise AssertionError("Invalid SQL Query") from e
    return sql_query


def upper_bound(x: np.ndarray, value: float) -> int:
    """
    Find the index of the last element which is less or equal to value
    """
    return int(np.argwhere(x <= value)[-1])
