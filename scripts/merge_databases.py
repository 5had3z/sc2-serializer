import sqlite3
import shutil
from pathlib import Path
import typer

app = typer.Typer()


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
def main(database_directory: Path, target_database: Path):
    # Assuming the table name is the same in all databases
    table_name = "game_data"

    # Loop through all databases in the directory
    for idx, source_database in enumerate(database_directory.glob("*.db")):
        if idx == 0:
            shutil.copy(source_database, target_database)
            continue
        # Merge the databases
        merge_databases(source_database, target_database, table_name)


if __name__ == "__main__":
    app()
