import torch
from torch import Tensor


def upper_bound(x: Tensor, value: float) -> int:
    """
    Find the index of the last element which is less or equal to value
    """
    return int(torch.argwhere(torch.le(x, value))[-1].item())
