import sqlite3
import typer
from typing_extensions import Annotated
from pathlib import Path
from sc2_replay_reader import Race, Result
from typing import List, Any, Callable, Dict, Tuple
from functools import reduce
import matplotlib.pyplot as plt

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


def get_rows(cursor: sqlite3.Cursor):
    query = """
        SELECT COUNT(*) as count
        FROM game_data
        """
    cursor.execute(query)

    # Fetch the results
    results = cursor.fetchone()

    typer.echo(f"Number of rows: {results[0]:,}")
    return results[0]


def count_invalid(cursor: sqlite3.Cursor, valid_rows: int | None = None):
    min_game_time_minutes = 10
    game_steps = int(min_game_time_minutes * 60 * 22.4)
    invalid_criteria = [
        "playerMMR < 0",
        "read_success = 0",
        "playerAPM < 0",
        "final_score_float < 0",
        f"game_length < {game_steps}",
    ]
    invalid_criteria.append(" OR ".join(invalid_criteria))
    for ic in invalid_criteria:
        query = f"""
            SELECT COUNT(*) as count
            FROM game_data
            WHERE {ic}
            """
        cursor.execute(query)

        # Fetch the results
        results = cursor.fetchone()

        if valid_rows is not None:
            typer.echo(
                f"Invalid rows ({ic}): {results[0]:,} ({results[0] * 100 / valid_rows:.2f}%)"
            )
        else:
            typer.echo(f"Invalid rows ({ic}): {results[0]:,}")


def count_discrete_values(
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


def generate_scatter(
    cursor: sqlite3.Cursor, column_x: str, column_y: str, x_filter=None, y_filter=None
):
    # Construct the SQL query with optional filters
    query = f"SELECT {column_y}, {column_x} FROM game_data"
    if x_filter is not None and y_filter is not None:
        query += f" WHERE {column_x} {x_filter} AND {column_y} {y_filter}"
    elif x_filter is not None:
        query += f" WHERE {column_x} {x_filter}"
    elif y_filter is not None:
        query += f" WHERE {column_y} {y_filter}"

    cursor.execute(query)
    data = cursor.fetchall()
    y_vals, x_vals = zip(*data)

    # Create a scatter plot
    plt.figure(figsize=(10, 6))
    plt.scatter(y_vals, x_vals, alpha=0.5, color="green", edgecolors="black")

    # Set labels and title
    plt.title(f"Scatter Plot of {column_y} vs {column_x}")
    plt.xlabel(column_y)
    plt.ylabel(column_x)

    plt.savefig(f"{column_y.replace(' ', '_')}_vs_{column_x.replace(' ', '_')}.png")


@app.command()
def main(database: Annotated[Path, typer.Option()]):
    database_str = str(database)

    # Use the SQLite connection
    try:
        conn = sqlite3.connect(database_str)
        cursor = conn.cursor()

        rows = get_rows(cursor)
        count_invalid(cursor, rows)

        count_discrete_values(cursor, {"playerRace": ([0, 1, 2], compose(Race, int))})
        count_discrete_values(
            cursor,
            {"playerRace": ([0, 1, 2], compose(Race, int)), "playerId": ([1, 2], None)},
        )
        count_discrete_values(
            cursor,
            {
                "playerRace": ([0, 1, 2], compose(Race, int)),
                "playerResult": ([0, 1], compose(Result, int)),
            },
        )
        count_discrete_values(
            cursor,
            {
                "playerId": ([1, 2], None),
                "playerResult": ([0, 1], compose(Result, int)),
            },
        )
        generate_scatter(cursor, "playerMMR", "game_length", x_filter="> 0")

        conn.close()
    except sqlite3.Error as e:
        typer.echo(f"SQLite error: {e}")


if __name__ == "__main__":
    app()
