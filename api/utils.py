from typing import Sequence

import torch


def find_closest_indices(options: Sequence[int], targets: Sequence[int]):
    """
    Find the closest option corresponding to a target, if there is no match, place -1
    """
    tgt_idx = 0
    nearest = torch.full([len(targets)], -1, dtype=torch.int32)
    for idx, (prv, nxt) in enumerate(zip(options, options[1:])):
        if prv > targets[tgt_idx]:  # not in between, skip
            tgt_idx += 1
            if tgt_idx == nearest.nelement():
                break
            continue
        if prv <= targets[tgt_idx] <= nxt:
            nearest[tgt_idx] = idx
            tgt_idx += 1
            if tgt_idx == nearest.nelement():
                break
    return nearest
