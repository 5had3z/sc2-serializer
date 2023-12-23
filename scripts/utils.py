from typing import Sequence

import torch
from torch import Tensor


def upper_bound(x: Tensor, value: float) -> int:
    """
    Find the index of the last element which is less or equal to value
    """
    return int(torch.argwhere(torch.le(x, value))[-1].item())


def find_closest_indicies(options: Sequence[int], targets: Sequence[int]):
    """
    Find the closest option corresponding to a target, if there is no match, place -1
    TODO Convert this to cpp
    """
    tgt_idx = 0
    nearest = torch.full([len(targets)], -1, dtype=torch.int32)
    for idx, (prv, nxt) in enumerate(zip(options, options[1:])):
        if prv <= targets[tgt_idx] and nxt >= targets[tgt_idx]:
            nearest[tgt_idx] = idx
            tgt_idx += 1
            if tgt_idx == nearest.nelement():
                break
    return nearest
