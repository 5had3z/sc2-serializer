import sqlite3
import typer
from typing_extensions import Annotated
from pathlib import Path
from sc2_replay_reader import Race, Result
from typing import List, Any, Callable, Dict, Tuple
from functools import reduce

app = typer.Typer()


def compose(*funcs):
    return lambda x: reduce(lambda acc, f: f(acc), reversed(funcs), x)


def get_races(cursor: sqlite3.Cursor):
    query = """
        SELECT playerrace, COUNT(*) as count
        FROM game_data
        WHERE playerrace IN (0, 1, 2)
        GROUP BY playerrace
        """
    cursor.execute(query)

    # Fetch the results
    results = cursor.fetchall()

    for row in results:
        playerrace, count = row
        typer.echo(f"Player Race {Race(int(playerrace))}: {count} occurrences")


def get_races_per_column(
    cursor: sqlite3.Cursor,
    columns: Dict[str, Tuple[List[Any], Callable[[str], Any] | None]],
):
    def convert(k, v):
        return f"{k} in ({','.join([str(i) for i in v[0]])})"

    query = f"""
            SELECT {", ".join(columns.keys())}, COUNT(*) as count
            FROM game_data
            WHERE {" AND ".join(convert(k,v) for k,v in columns.items())}
            GROUP BY {", ".join(columns.keys())}
        """
    cursor.execute(query)

    # Fetch the results
    results = cursor.fetchall()

    for row in results:
        # Extract column values and count from the row
        values = row[:-1]
        count = row[-1]

        # Format and print the output dynamically
        formatted_values = ", ".join(
            f"{k}: {v[1](str(val)) if v[1] is not None else val}"
            for (k, v), val in zip(columns.items(), values)
        )

        typer.echo(f"{formatted_values}, Count: {count} occurrences")


@app.command()
def main(database: Annotated[Path, typer.Option()]):
    database_str = str(database)

    # Use the SQLite connection
    try:
        conn = sqlite3.connect(database_str)
        cursor = conn.cursor()

        get_races_per_column(cursor, {"playerRace": ([0, 1, 2], compose(Race, int))})
        get_races_per_column(
            cursor,
            {"playerRace": ([0, 1, 2], compose(Race, int)), "playerId": ([1, 2], None)},
        )
        get_races_per_column(
            cursor,
            {
                "playerRace": ([0, 1, 2], compose(Race, int)),
                "playerResult": ([0, 1], compose(Result, int)),
            },
        )
        get_races_per_column(
            cursor,
            {
                "playerId": ([1, 2], None),
                "playerResult": ([0, 1], compose(Result, int)),
            },
        )

        conn.close()
    except sqlite3.Error as e:
        typer.echo(f"SQLite error: {e}")


if __name__ == "__main__":
    app()
