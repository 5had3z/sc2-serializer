from pathlib import Path
from typing import Annotated

import numpy as np
import typer
from _womd_binding import SequenceData, WomdDatabase, parseSequenceFromArray
from datapipe import womd_pipeline
from nvidia.dali.plugin.pytorch import DALIGenericIterator
from rich.progress import track
from torch import Tensor

app = typer.Typer()


def split_data_mask(data: np.ndarray) -> tuple[np.ndarray, np.ndarray]:
    """Split input into data and mask"""
    return data[..., :-1], data[..., -1].astype(np.uint8)


def tensor_to_string(data: Tensor) -> str:
    """Convert tensor to string"""
    return "".join([chr(int(c)) for c in data.to().tolist()])


def parse_sample_to_sequence(sample: dict[str, Tensor]) -> SequenceData:
    """Convert data yielded by DALI to SequenceData"""
    agents, agents_mask = split_data_mask(sample["state"].numpy())
    traffic, traffic_mask = split_data_mask(sample["signals"].numpy())
    road, road_mask = split_data_mask(sample["road_graph"].numpy())
    sequence = parseSequenceFromArray(
        agents,
        agents_mask,
        traffic,
        traffic_mask,
        road,
        road_mask,
        tensor_to_string(sample["scenario_id"]),
    )
    return sequence


@app.command()
def main(
    womd: Annotated[Path, typer.Option()], output: Annotated[Path, typer.Option()]
) -> None:
    """Run conversion of Womd to Serializer format"""
    batch_size = 1
    datapipe = womd_pipeline(
        womd, num_threads=2, device_id=0, batch_size=batch_size, debug=True
    )
    dataloader = DALIGenericIterator(
        datapipe,
        ["state", "signals", "road_graph", "scenario_id"],
        reader_name="womd-reader",
    )

    database = WomdDatabase(output)

    for record in track(dataloader, total=len(dataloader)):
        batch = record[0]
        for bidx in range(batch_size):
            sample = {k: v[bidx] for k, v in batch.items()}
            sequence = parse_sample_to_sequence(sample)
            ok = database.addEntry(sequence)
            if not ok:
                raise RuntimeError("Failed to add entry to database")


if __name__ == "__main__":
    app()
