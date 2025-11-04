import time
from pathlib import Path
from typing import Annotated

import numpy as np
import typer
from _womd_binding import SequenceData, WomdDatabase, parseSequenceFromArray
from datapipe import womd_pipeline
from matplotlib import pyplot as plt
from nvidia.dali.plugin.pytorch import DALIGenericIterator
from rich.progress import track
from torch import Tensor

app = typer.Typer()


def split_data_mask(data: np.ndarray) -> tuple[np.ndarray, np.ndarray]:
    """Split input into data and mask"""
    return data[..., :-1], data[..., -1].astype(np.uint8)


def tensor_to_string(data: Tensor) -> str:
    """Convert tensor to string"""
    return "".join([chr(int(c)) for c in data.tolist()])


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
def convert(
    womd: Annotated[Path, typer.Option()], output: Annotated[Path, typer.Option()]
) -> None:
    """Run conversion of Womd to Serializer format"""
    batch_size = 1
    datapipe = womd_pipeline(womd, num_threads=2, device_id=0, batch_size=batch_size)
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


@app.command()
def benchmark(
    custom: Annotated[Path, typer.Option()], tfrecord: Annotated[Path, typer.Option()]
):
    """Compare the time it takes to fully read from custom format vs tfrecord dataloader"""
    datapipe = womd_pipeline(tfrecord, num_threads=1, device_id=0, batch_size=1)
    dataloader = DALIGenericIterator(
        datapipe,
        ["state", "signals", "road_graph", "scenario_id"],
        reader_name="womd-reader",
    )

    start = time.time()
    for _ in track(dataloader, "tfrecord", len(dataloader)):
        pass
    tfrecord_time = time.time() - start

    database = WomdDatabase()
    database.open(custom)
    num_samples = len(database)
    start = time.time()
    for i in track(range(num_samples), "custom", num_samples):
        _ = database[i]
    custom_time = time.time() - start

    print(f"{tfrecord_time=:.3f} vs {custom_time=:.3f}")


@app.command()
def show_womd_sample(path: Path, index: int = 1, out: Path | None = None):
    """Show the splines of roadgraph features, traffic lights and vehicle trajectories"""
    if not path.exists():
        raise FileNotFoundError(path)

    database = WomdDatabase()
    database.open(path)
    sample = database[index]

    fig, ax = plt.subplots(1, 1)
    fig.set_figheight(20)
    fig.set_figwidth(20)
    agent_x = []
    agent_y = []
    sdc_x = []
    sdc_y = []
    for agents_ts in sample.agentData:
        for agent in agents_ts:
            if agent.is_sdc:
                sdc_x.append(agent.x)
                sdc_y.append(agent.y)
            else:
                agent_x.append(agent.x)
                agent_y.append(agent.y)
    ax.scatter(agent_x, agent_y, c="blue")
    ax.scatter(sdc_x, sdc_y, c="orange")

    signal_x = []
    signal_y = []
    for signal_ts in sample.signalsData:
        for signal in signal_ts:
            signal_x.append(signal.x)
            signal_y.append(signal.y)
    ax.scatter(signal_x, signal_y, c="red")

    rg_ids = np.array(sample.roadGraph.id)
    rg_xy = np.array([sample.roadGraph.x, sample.roadGraph.y])
    for uid in np.unique(rg_ids):
        xy = rg_xy[:, rg_ids == uid]
        ax.plot(xy[0], xy[1])

    if out is not None:
        fig.savefig(out)
    else:
        fig.show()


if __name__ == "__main__":
    app()
