import os
import sqlite3
from pathlib import Path
from typing import Any, Dict, Tuple

import torch
import typer
from torch.utils.data import DataLoader
from tqdm import tqdm
from typing_extensions import Annotated

from sc2_replay_reader import Score
from summaryStats import SQL_TYPES, LambdaFunctionType, SC2Replay

app = typer.Typer()


def custom_collate(batch):
    # No read success in entire batch
    if not any(item["read_success"] for item in batch):
        raise Exception(
            f"Nothing successful in entire batch of length {len(batch)}, try making it larger"
        )
    if all(item["read_success"] for item in batch):
        return torch.utils.data.dataloader.default_collate(batch)

    first_read_success = next((item for item in batch if item.get("read_success")))
    extra_keys = set(first_read_success.keys()) - {"partition", "idx", "read_success"}

    # Create a dictionary with zero tensors for extra_keys
    empty_batch = {key: 0 for key in extra_keys}
    data_batch = [
        {**empty_batch, **data} if not data["read_success"] else data for data in batch
    ]

    return torch.utils.data.dataloader.default_collate(data_batch)


def make_database(
    path: Path,
    additional_columns: Dict[str, SQL_TYPES],
    features: Dict[str, SQL_TYPES],
    lambda_columns: Dict[str, Tuple[SQL_TYPES, LambdaFunctionType]],
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
    return conn, cursor


def close_database(conn: sqlite3.Connection):
    # Commit the changes and close the connection
    conn.commit()
    conn.close()


def add_to_database(cursor: sqlite3.Cursor, data_dict: Dict[str, Any]):
    columns = ", ".join(data_dict.keys())
    placeholders = ", ".join("?" for _ in data_dict.values())

    query = f"""
        INSERT INTO game_data ({columns})
        VALUES ({placeholders})
    """

    cursor.execute(query, tuple(data_dict.values()))


@app.command()
def main(
    workspace: Annotated[Path, typer.Option()] = Path("."),
    workers: Annotated[int, typer.Option()] = 0,
):
    features: Dict[str, SQL_TYPES] = {
        "replayHash": "TEXT",
        "gameVersion": "TEXT",
        "playerId": "INTEGER",
        "playerRace": "TEXT",
        "playerResult": "TEXT",
        "playerMMR": "INTEGER",
        "playerAPM": "INTEGER",
    }
    # Manually include additional columns
    additional_columns: Dict[str, SQL_TYPES] = {
        "partition": "TEXT",
        "idx": "INTEGER",
        "read_success": "BOOLEAN",
    }

    all_attributes = [
        attr
        for attr in dir(Score)
        if not callable(getattr(Score, attr))
        if "__" not in attr
    ]

    lambda_columns: Dict[str, Tuple[SQL_TYPES, LambdaFunctionType]] = {
        "max_units": ("TEXT", lambda y: max(len(x) for x in y.data.units)),
        "game_length": ("INTEGER", lambda y: (y.data.gameStep[-1])),
        **{
            f"final_{i}": (
                "FLOAT",
                (lambda k: lambda y, key=k: float(getattr(y.data.score[-1], key)))(i),
            )
            for i in all_attributes
        },
    }

    conn, cursor = make_database(
        workspace / "gamedata.db", additional_columns, features, lambda_columns
    )
    if "POD_NAME" in os.environ:
        number = os.environ["POD_NAME"].split("_")[-1]
        dataset = SC2Replay(
            Path(os.environ["DATAPATH"]) / f"db_{number}.SC2Replays",
            set(features.keys()),
            lambda_columns,
        )
    else:
        dataset = SC2Replay(
            Path(os.environ["DATAPATH"]), set(features.keys()), lambda_columns
        )
    batch_size = 50
    dataloader = DataLoader(
        dataset, num_workers=workers, batch_size=batch_size, collate_fn=custom_collate
    )
    for idx, d in tqdm(enumerate(dataloader), total=len(dataloader)):
        keys = d.keys()
        for index in range(len(d["partition"])):
            converted_d = {}
            for key in keys:
                value = d[key]

                if isinstance(value, torch.Tensor):
                    converted_d[key] = value[index].item()
                elif isinstance(value, list):
                    converted_d[key] = value[index]

            add_to_database(cursor, converted_d)

        if idx % 5 == 0:
            conn.commit()


if __name__ == "__main__":
    app()
