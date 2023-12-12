import sqlite3
from summaryStats import SC2Replay, LambdaFunctionType, SQL_TYPES
import os
from pathlib import Path
from torch.utils.data import DataLoader
import torch
from sc2_replay_reader import Score
from typing import Dict, Tuple, Any
import typer
from typing_extensions import Annotated

app = typer.Typer()


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
    workspace: Annotated[Path, typer.Option()], workers: Annotated[int, typer.Option()]
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
    additional_columns: Dict[str, SQL_TYPES] = {"partition": "TEXT", "idx": "INTEGER"}

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

    dataset = SC2Replay(
        Path(os.environ["DATAPATH"]), set(features.keys()), lambda_columns
    )
    dataloader = DataLoader(dataset, num_workers=workers, batch_size=1)
    for idx, d in enumerate(dataloader):
        converted_d = {}

        for key, value in d.items():
            assert len(value) == 1
            if isinstance(value, torch.Tensor):
                converted_d[key] = value[0].item()
            elif isinstance(value, list):
                converted_d[key] = value[0]
        if idx % 5 == 0:
            conn.commit()

        add_to_database(cursor, converted_d)


if __name__ == "__main__":
    app()
