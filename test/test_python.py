import os
from pathlib import Path

import pytest
from sc2_serializer.example import SC2Dataset
from sc2_serializer.sampler import BasicSampler, SQLSampler
from sc2_serializer import get_database_and_parser, StepDataSoA

_DEFAULT_REPLAYS = "/mnt/datasets/sc2-tournament"


@pytest.mark.parametrize("has_units", [True, False])
@pytest.mark.parametrize("has_minimaps", [True, False])
def test_replay_database_ops(has_units: bool, has_minimaps: bool):
    """Test basic functionality of python bindings to replay database"""
    db, parser = get_database_and_parser(has_units, has_minimaps)
    replays_path = Path(os.environ.get("DATAPATH", _DEFAULT_REPLAYS))
    replay_file = next(
        filter(lambda x: x.suffix == ".SC2Replays", replays_path.iterdir())
    )
    db.load(replay_file)
    for idx in [0, len(db) // 2, len(db) - 1]:
        replay1 = db[idx]
        replay2 = db.getEntry(idx)
        assert replay1.header == replay2.header
        assert len(replay1) > 0 and len(replay2) > 0
        if isinstance(replay1.data, StepDataSoA):
            assert isinstance(replay2.data, StepDataSoA)
            assert replay1.data.units == replay2.data.units

        parser.parse_replay(replay1)


_REPLAYS_PATH = Path(os.environ.get("DATAPATH", _DEFAULT_REPLAYS))


@pytest.mark.parametrize(
    "sampler",
    [
        SQLSampler(
            "$ENV:gamedata.db",
            _REPLAYS_PATH,
            [
                "game_length > 6720",
                "parse_success = 1",
                "number_game_step > 1024",
                "playerAPM > 100",
            ],
            train_ratio=0.8,
            is_train=True,
        ),
        BasicSampler(_REPLAYS_PATH, train_ratio=0.8, is_train=True),
    ],
)
def test_pytorch_example(sampler):
    """Test able to yield data from the example dataset, some sample data might have issues so
    might have to attempt a few times (e.g. missing alert data on older game versions).
    """
    dataset = SC2Dataset(sampler, features=["minimaps", "scalars"])

    tries = 0
    MAX_TRY = 10
    while tries < MAX_TRY:
        try:
            idx = tries * (len(dataset) // MAX_TRY)
            test_sample = dataset[idx]
            assert test_sample
        except RuntimeError as err:
            if str(err) == "Tried to get alerts data but it was empty":
                tries += 1
            else:
                raise RuntimeError from err
        else:
            break

    if tries >= MAX_TRY:
        raise RuntimeError("FAILED TO READ ANY REPLAY")

    with pytest.raises(IndexError):
        test_sample = sampler[len(sampler)]

    with pytest.raises(IndexError):
        test_sample = dataset[len(dataset)]
