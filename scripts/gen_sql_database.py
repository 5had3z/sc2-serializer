import os
import shutil
import sqlite3
from pathlib import Path
from typing import Annotated, Any, Literal, Callable

import torch
import typer
from torch import Tensor
from torch.utils.data import DataLoader, Dataset
from torch.utils.data._utils.collate import default_collate
from sc2_replay_reader import (
    Score,
    GAME_INFO_FILE,
    ReplayDataAllDatabase,
    ReplayDataAllParser,
    set_replay_database_logger_level,
    spdlog_lvl,
)

app = typer.Typer()

SQL_TYPES = Literal["INTEGER", "FLOAT", "TEXT", "BOOLEAN"]
ENUM_KEYS = {"playerRace", "playerResult"}
LambdaFunctionType = Callable[[ReplayDataAllParser], float | int]

FEATURES: dict[str, SQL_TYPES] = {
    "replayHash": "TEXT",
    "gameVersion": "TEXT",
    "playerId": "INTEGER",
    "playerRace": "TEXT",
    "playerResult": "TEXT",
    "playerMMR": "INTEGER",
    "playerAPM": "INTEGER",
}
# Manually include additional columns
ADDITIONAL_COLUMNS: dict[str, SQL_TYPES] = {
    "partition": "TEXT",
    "idx": "INTEGER",
    "read_success": "BOOLEAN",
    "parse_success": "BOOLEAN",
}

ALL_ATTRIBUTES = [
    attr
    for attr in dir(Score)
    if not callable(getattr(Score, attr))
    if "__" not in attr
]

LAMBDA_COLUMNS: dict[str, tuple[SQL_TYPES, LambdaFunctionType]] = {
    "max_units": ("TEXT", lambda y: max(len(x) for x in y.data.units)),
    "game_length": ("INTEGER", lambda y: (y.data.gameStep[-1])),
    **{
        f"final_{i}": (
            "FLOAT",
            (lambda k: lambda y, key=k: float(getattr(y.data.score[-1], key)))(i),
        )
        for i in ALL_ATTRIBUTES
    },
}


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


# Function to merge databases
def merge_databases(source_db, target_db, table_name):
    # Connect to the source database
    source_conn = sqlite3.connect(source_db)
    source_cursor = source_conn.cursor()

    # Get column names from the source database
    source_cursor.execute(f"PRAGMA table_info({table_name})")
    columns = [column[1] for column in source_cursor.fetchall()]

    # Connect to the target database and create the table if it doesn't exist
    target_conn = sqlite3.connect(target_db)
    target_cursor = target_conn.cursor()

    # Generate the INSERT statement dynamically based on the columns
    columns_str = ", ".join(columns)
    placeholders_str = ", ".join(["?" for _ in columns])

    insert_statement = (
        f"INSERT INTO {table_name} " f"({columns_str}) " f"VALUES ({placeholders_str})"
    )

    # Fetch data from the source database
    source_cursor.execute(f"SELECT * FROM {table_name}")
    data_from_source = source_cursor.fetchall()

    # Insert data into the target database
    for row in data_from_source:
        target_cursor.execute(insert_statement, row)

    # Commit changes and close connections
    target_conn.commit()
    target_cursor.close()
    target_conn.close()

    source_cursor.close()
    source_conn.close()


@app.command()
def merge(directory: Path, target: Path):
    """Merge all databases in diretory to target"""
    # Assuming the table name is the same in all databases
    table_name = "game_data"

    # Loop through all databases in the directory
    for idx, source_database in enumerate(directory.glob("*.db")):
        if idx == 0:
            shutil.copyfile(source_database, target)
            continue
        # Merge the databases
        merge_databases(source_database, target, table_name)


def custom_collate(batch):
    # No read success in entire batch
    if not any(item["read_success"] for item in batch):
        return default_collate(batch)

    if all(item["read_success"] for item in batch):
        return default_collate(batch)

    first_read_success = next((item for item in batch if item.get("read_success")))
    extra_keys = set(first_read_success.keys()) - {
        "partition",
        "idx",
        "read_success",
        "parse_success",
    }

    # Create a dictionary with zero tensors for extra_keys
    empty_batch = {key: 0 for key in extra_keys}
    data_batch = [
        {**empty_batch, **data} if not data["read_success"] else data for data in batch
    ]

    return default_collate(data_batch)


def make_database(
    path: Path,
    additional_columns: dict[str, SQL_TYPES],
    features: dict[str, SQL_TYPES],
    lambda_columns: dict[str, tuple[SQL_TYPES, LambdaFunctionType]],
):
    if path.exists():
        os.remove(path)
    # Connect to the SQLite database (creates a new database if it doesn't exist)
    conn = sqlite3.connect(str(path))

    # Create a cursor object to execute SQL commands
    cursor = conn.cursor()

    # Create a table with the specified headings and data types
    create_table_sql = f"""
        CREATE TABLE game_data (
            {', '.join(f"{column} {datatype}" for column, datatype in additional_columns.items())},
            {', '.join(f"{column} {datatype}" for column, datatype in features.items())},
            {', '.join(f"{column} {datatype}" for column, (datatype, _) in lambda_columns.items())}
        )
    """
    cursor.execute(create_table_sql)
    cursor.close()
    return conn


def add_to_database(cursor: sqlite3.Cursor, data_dict: dict[str, Any]):
    columns = ", ".join(data_dict.keys())
    placeholders = ", ".join("?" for _ in data_dict.values())

    query = f"""
        INSERT INTO game_data ({columns})
        VALUES ({placeholders})
    """

    cursor.execute(query, tuple(data_dict.values()))


@app.command()
def create_individual(
    workspace: Annotated[Path, typer.Option()] = Path("."),
    workers: Annotated[int, typer.Option()] = 0,
):
    """Create sql databases for each individual replay database"""
    replay_files = list(Path(os.environ["DATAPATH"]).glob("*.SC2Replays"))
    for p in replay_files:
        try:
            os.environ["DATAPATH"] = str(p)  # Override DATAPATH with individual file
            create(workspace=workspace, workers=workers, name=p.name)
        except Exception as e:
            print(f"Failed Replay {p.name} with exception: {e}")


def make_standard(workspace: Path, name: str):
    """Create dataset and sql from standard replay dataset"""
    dataset = SC2Replay(
        Path(os.environ["DATAPATH"]), set(FEATURES.keys()), LAMBDA_COLUMNS
    )
    conn = make_database(
        workspace / f"{name}.db", ADDITIONAL_COLUMNS, FEATURES, LAMBDA_COLUMNS
    )
    return dataset, conn


def make_kubernetes(workspace: Path, name: str, pod_offset: int):
    """Create sql database per pod index, replay database must be indexed"""
    number = int(os.environ["POD_NAME"].split("-")[-1]) + pod_offset
    dataset = SC2Replay(
        Path(os.environ["DATAPATH"]) / f"db_{number}.SC2Replays",
        set(FEATURES.keys()),
        LAMBDA_COLUMNS,
    )
    db_file = workspace / f"{name}_{number}.db"
    if db_file.is_file():
        file_size_bytes = db_file.stat().st_size

        # Convert bytes to megabytes
        file_size_mb = file_size_bytes / (1024 * 1024)
        print(f"Found existing file: {db_file} with size {file_size_mb}MB")
        if file_size_mb > 1:
            print("skipping file which exists....")
            return

    conn = make_database(db_file, ADDITIONAL_COLUMNS, FEATURES, LAMBDA_COLUMNS)
    return dataset, conn


@app.command()
def create(
    workspace: Annotated[Path, typer.Option()] = Path().cwd(),
    workers: Annotated[int, typer.Option()] = 0,
    name: Annotated[str, typer.Option()] = "gamedata",
    pod_offset: Annotated[int, typer.Option(help="Apply offset to k8s pod index")] = 0,
):
    """Standard creation function to create sql database from dataloader"""
    if "POD_NAME" in os.environ:
        dataset, conn = make_kubernetes(workspace, name, pod_offset)
    else:
        dataset, conn = make_standard(workspace, name)

    dataloader = DataLoader(
        dataset, num_workers=workers, batch_size=32, collate_fn=custom_collate
    )

    cursor = conn.cursor()
    for idx, d in typer.progressbar(enumerate(dataloader), length=len(dataloader)):
        keys = d.keys()
        for index in range(len(d["partition"])):
            converted_d = {}
            for key in keys:
                value = d[key]
                if isinstance(value, Tensor):
                    converted_d[key] = value[index].item()
                elif isinstance(value, list):
                    converted_d[key] = value[index]

            add_to_database(cursor, converted_d)

        if idx % 5 == 0:
            conn.commit()

    cursor.close()
    conn.commit()
    conn.close()


if __name__ == "__main__":
    app()
