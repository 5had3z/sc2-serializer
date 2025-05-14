"""
pbdoc(Python bindings for Waymo Open Motion Data with Serializer)pbdoc
"""

from __future__ import annotations

import os
import typing

import numpy as np

__all__ = [
    "Agent",
    "RoadGraph",
    "SequenceData",
    "TrafficLight",
    "WomdDatabase",
    "parseSequenceFromArray",
]

class Agent:
    bbox_yaw: float
    height: float
    id: float
    is_sdc: bool
    length: float
    timestamp_micros: int
    tracks_to_predict: int
    type: float
    vel_yaw: float
    velocity_x: float
    velocity_y: float
    width: float
    x: float
    y: float
    z: float
    def __init__(self) -> None: ...

class RoadGraph:
    dir: list[float]
    id: list[int]
    type: list[int]
    x: list[float]
    y: list[float]
    z: list[float]
    def __init__(self) -> None: ...

class SequenceData:
    agentData: list[list[Agent]]
    roadGraph: RoadGraph
    scenarioId: str
    signalsData: list[list[TrafficLight]]
    def __init__(self) -> None: ...

class TrafficLight:
    state: int
    x: float
    y: float
    z: float
    def __init__(self) -> None: ...

class WomdDatabase:
    def __getitem__(self, index: int) -> SequenceData: ...
    @typing.overload
    def __init__(self) -> None: ...
    @typing.overload
    def __init__(self, dbPath: os.PathLike) -> None: ...
    def __len__(self) -> int: ...
    def addEntry(self, data: SequenceData) -> bool: ...
    def create(self, dbPath: os.PathLike) -> bool: ...
    def getEntry(self, index: int) -> SequenceData: ...
    def getEntryUID(self, index: int) -> str: ...
    def getHeader(self, index: int) -> str: ...
    def isFull(self) -> bool: ...
    def load(self, dbPath: os.PathLike) -> bool: ...
    def open(self, dbPath: os.PathLike) -> bool: ...
    def size(self) -> int: ...
    @property
    def path(self) -> os.PathLike: ...

def parseSequenceFromArray(
    agents: np.ndarray[np.float32],
    agents_mask: np.ndarray[np.uint8],
    traffic: np.ndarray[np.float32],
    traffic_mask: np.ndarray[np.uint8],
    roadgraph: np.ndarray[np.float32],
    roadgraph_mask: np.ndarray[np.uint8],
    scenarioId: str,
) -> SequenceData: ...

__version__: str = "0.0.1"
