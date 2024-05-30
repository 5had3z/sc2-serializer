"""
Basic example PyTorch Dataloader, a more complex dataloader can be found in the
experiment code accompanying the paper at https://github.com/5had3z/sc2-experiments.
"""

from dataclasses import dataclass
from typing import Sequence

import numpy as np
import torch
from torch.utils.data import Dataset

from . import get_database_and_parser, Result
from .sampler import ReplaySampler


@dataclass
class TimeRange:
    """Parameters for a range of time points"""

    min: float
    max: float
    step: float

    def __post_init__(self):
        assert self.min < self.max

    def arange(self):
        """Return arange instance of parameters"""
        return torch.arange(self.min, self.max, self.step)


def find_closest_indices(options: Sequence[int], targets: Sequence[int]):
    """
    Find the closest option corresponding to a target, if there is no match, place -1
    """
    tgt_idx = 0
    nearest = torch.full([len(targets)], -1, dtype=torch.int32)
    for idx, (prv, nxt) in enumerate(zip(options, options[1:])):
        if prv > targets[tgt_idx]:  # not in between, skip
            tgt_idx += 1
        elif prv <= targets[tgt_idx] <= nxt:
            nearest[tgt_idx] = idx
            tgt_idx += 1
        if tgt_idx == nearest.nelement():
            break
    return nearest


class SC2Dataset(Dataset):
    """Example PyTorch Dataset for StarCraft II Data.

    This sample dataloader reads a replay around the timepoint range specified and returns
    the requested [scalar, minimap] features and the game outcome from the player POV.

    Args:
        sampler (ReplaySampler): Sampler that yields a replay file and index in that file.
        features (list[str], optional): List of step data features to use. (default: [minimaps, scalars])
        timepoints (TimeRange): Timepoints in the replay to sample from.
    """

    def __init__(
        self,
        sampler: ReplaySampler,
        features: list[str] | None = None,
        timepoints: TimeRange = TimeRange(0, 30, 0.5),
    ):
        self.sampler = sampler
        self.features = ["minimaps", "scalars"] if features is None else features
        self.db_handle, self.parser = get_database_and_parser(
            parse_units=False, parse_minimaps="minimaps" in self.features
        )

        _loop_per_min = 22.4 * 60
        self._target_game_loops = (timepoints.arange() * _loop_per_min).to(torch.int)

    def __len__(self):
        return len(self.sampler)

    def __getitem__(self, index: int):
        """Sample from dataset at index.

        Args:
            index (int): Index to forward to ReplaySampler

        Return:
            dict [str, Tensor]: observation data, True if valid mask, and game outcome
        """
        filepath, db_index = self.sampler.sample(index)

        assert self.db_handle.load(filepath), f"Failed to load {filepath}"
        if db_index >= len(self.db_handle):
            raise IndexError(f"{db_index} exceeds {len(self.db_handle)}")

        self.parser.parse_replay(self.db_handle.getEntry(db_index))
        return self.load_from_parser()

    def load_from_parser(self):
        """Sample from data currently in parser

        Return:
            dict [str, Tensor]: observation data, True if valid mask, and game outcome
        """
        outputs_list = self.parser.sample(0)
        outputs_list = {k: [outputs_list[k]] for k in self.features}

        sample_indices = find_closest_indices(
            self.parser.data.gameStep, self._target_game_loops[1:]
        )
        for idx in sample_indices:
            if idx == -1:
                sample = {k: np.zeros_like(outputs_list[k][-1]) for k in outputs_list}
            else:
                sample = self.parser.sample(int(idx.item()))
            for k in outputs_list:
                outputs_list[k].append(sample[k])

        outputs = {
            "win": torch.as_tensor(
                self.parser.info.playerResult == Result.Win, dtype=torch.float32
            ),
            "valid": torch.cat([torch.tensor([True]), sample_indices != -1]),
        }
        for k in outputs_list:
            outputs[k] = torch.stack([torch.as_tensor(o) for o in outputs_list[k]])

        return outputs
