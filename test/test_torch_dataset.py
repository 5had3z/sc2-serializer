import os
from pathlib import Path

from sc2_replay_reader.example import SC2Dataset
from sc2_replay_reader.sampler import SQLSampler


def test_sql_example():
    """Test able to yield data from the example dataset"""
    filters = [
        "game_length > 6720",
        "parse_success = 1",
        "number_game_step > 1024",
        "playerAPM > 100",
    ]
    replays_path = Path(os.environ.get("DATAPATH", "/mnt/datasets/sc2-tournament"))
    sampler = SQLSampler(
        "gamedata.db", replays_path, filters, train_ratio=0.8, is_train=True
    )
    dataset = SC2Dataset(sampler, features={"minimaps", "scalars"})
    test_sample = dataset[0]
    assert test_sample
