import logging
from pathlib import Path
from subprocess import run
from multiprocessing import Pool, cpu_count

import nvidia.dali.tfrecord as tfrec
from nvidia.dali.pipeline import pipeline_def
from nvidia.dali import fn
from nvidia.dali.types import DALIDataType
from nvidia.dali.data_node import DataNode


def get_tfrecord_cache(record_root: Path, tfrecords: list[str]) -> list[str]:
    """Get the list of paths of tfrecord indexes
    and create the index if necessary"""
    # Get the path to the record index folder
    tfrec_idx_root = record_root.parent / f"{record_root.name}_dali_idx"
    tfrec_idx_root.mkdir(exist_ok=True)

    # Create the path to each index
    tfrec_idxs = [tfrec_idx_root / f"{tfrec}.idx" for tfrec in tfrecords]

    # Check if index exists, write if necessary
    proc_args = [
        ["tfrecord2idx", str(record_root / tfrec), str(idx)]
        for tfrec, idx in zip(tfrecords, tfrec_idxs)
        if not idx.exists()
    ]

    if len(proc_args) > 0:
        logging.info("Creating %d DALI TFRecord Indexes", len(proc_args))
        with Pool(processes=cpu_count() // 2) as mp:
            mp.map(run, proc_args)

    return [str(f) for f in tfrec_idxs]


def cat_features(
    features: dict[str, DataNode], prefix: str, keys: list[str]
) -> DataNode:
    """Concatenate features with the same prefix in the order defined by keys"""
    return fn.cat(*[features[f"{prefix}/{key}"] for key in keys], axis=-1)


def stack_features(
    features: dict[str, DataNode], prefix: str, keys: list[str]
) -> DataNode:
    """Concatenate features with the same prefix in the order defined by keys"""
    return fn.stack(*[features[f"{prefix}/{key}"] for key in keys], axis=-1)


def concatenate_time(features: dict[str, DataNode]):
    """Concatenate time features, mutates features in place, removes old keys.

    For example, transforms prefix/{past,current,future}/suffix into prefix/suffix.
    """
    keys_to_stack = set(
        map(
            lambda x: f"{x[0]}/{x[2]}",
            filter(
                lambda x: x[1] in {"future", "past", "current"},
                map(lambda x: x.split("/"), features),
            ),
        )
    )

    for key in keys_to_stack:
        pre, post = key.split("/")
        time_keys = [f"{pre}/{t}/{post}" for t in ["past", "current", "future"]]
        features[key] = fn.cat(
            *[features[k] for k in time_keys], axis=1 if key.startswith("state") else 0
        )
        for k in time_keys:
            del features[k]


def transpose_key(features: dict[str, DataNode], prefix: str, perm: list[int] | int):
    """Transpose features that match prefix with permutation perm, mutates features in place."""
    for key in features:
        if key.startswith(prefix):
            features[key] = fn.transpose(features[key], perm=perm)


def expand_feature_over_sequence(features: dict[str, DataNode], keys: list[str]):
    """Expand features with the same prefix from [128] to [128,91,1]"""
    for key in keys:
        features[key] = fn.stack(*[features[key]] * 91, axis=-1)


@pipeline_def
def womd_pipeline(record_path: Path):
    # fmt: off
    # Features of the road.
    roadgraph_features = {
        "roadgraph_samples/dir": tfrec.FixedLenFeature([20000, 3], tfrec.float32, 0.0),
        "roadgraph_samples/id": tfrec.FixedLenFeature([20000, 1], tfrec.int64, 0),
        "roadgraph_samples/type": tfrec.FixedLenFeature([20000, 1], tfrec.int64, 0),
        "roadgraph_samples/valid": tfrec.FixedLenFeature([20000, 1], tfrec.int64, 0),
        "roadgraph_samples/xyz": tfrec.FixedLenFeature([20000, 3], tfrec.float32, 0.0),
    }

    # Features of other agents.
    state_features = {
        "state/id": tfrec.FixedLenFeature([128], tfrec.float32, 0.0),
        "state/type": tfrec.FixedLenFeature([128], tfrec.float32, 0.0),
        "state/is_sdc": tfrec.FixedLenFeature([128], tfrec.int64, 0),
        "state/tracks_to_predict": tfrec.FixedLenFeature([128], tfrec.int64, 0),
        "state/current/bbox_yaw": tfrec.FixedLenFeature([128, 1], tfrec.float32, 0.0),
        "state/current/height": tfrec.FixedLenFeature([128, 1], tfrec.float32, 0.0),
        "state/current/length": tfrec.FixedLenFeature([128, 1], tfrec.float32, 0.0),
        "state/current/timestamp_micros": tfrec.FixedLenFeature([128, 1], tfrec.int64, 0),
        "state/current/valid": tfrec.FixedLenFeature([128, 1], tfrec.int64, 0),
        "state/current/vel_yaw": tfrec.FixedLenFeature([128, 1], tfrec.float32, 0.0),
        "state/current/velocity_x": tfrec.FixedLenFeature([128, 1], tfrec.float32, 0.0),
        "state/current/velocity_y": tfrec.FixedLenFeature([128, 1], tfrec.float32, 0.0),
        "state/current/width": tfrec.FixedLenFeature([128, 1], tfrec.float32, 0.0),
        "state/current/x": tfrec.FixedLenFeature([128, 1], tfrec.float32, 0.0),
        "state/current/y": tfrec.FixedLenFeature([128, 1], tfrec.float32, 0.0),
        "state/current/z": tfrec.FixedLenFeature([128, 1], tfrec.float32, 0.0),
        "state/future/bbox_yaw": tfrec.FixedLenFeature([128, 80], tfrec.float32, 0.0),
        "state/future/height": tfrec.FixedLenFeature([128, 80], tfrec.float32, 0.0),
        "state/future/length": tfrec.FixedLenFeature([128, 80], tfrec.float32, 0.0),
        "state/future/timestamp_micros": tfrec.FixedLenFeature([128, 80], tfrec.int64, 0),
        "state/future/valid": tfrec.FixedLenFeature([128, 80], tfrec.int64, 0),
        "state/future/vel_yaw": tfrec.FixedLenFeature([128, 80], tfrec.float32, 0.0),
        "state/future/velocity_x": tfrec.FixedLenFeature([128, 80], tfrec.float32, 0.0),
        "state/future/velocity_y": tfrec.FixedLenFeature([128, 80], tfrec.float32, 0.0),
        "state/future/width": tfrec.FixedLenFeature([128, 80], tfrec.float32, 0.0),
        "state/future/x": tfrec.FixedLenFeature([128, 80], tfrec.float32, 0.0),
        "state/future/y": tfrec.FixedLenFeature([128, 80], tfrec.float32, 0.0),
        "state/future/z": tfrec.FixedLenFeature([128, 80], tfrec.float32, 0.0),
        "state/past/bbox_yaw": tfrec.FixedLenFeature([128, 10], tfrec.float32, 0.0),
        "state/past/height": tfrec.FixedLenFeature([128, 10], tfrec.float32, 0.0),
        "state/past/length": tfrec.FixedLenFeature([128, 10], tfrec.float32, 0.0),
        "state/past/timestamp_micros": tfrec.FixedLenFeature([128, 10], tfrec.int64, 0),
        "state/past/valid": tfrec.FixedLenFeature([128, 10], tfrec.int64, 0),
        "state/past/vel_yaw": tfrec.FixedLenFeature([128, 10], tfrec.float32, 0.0),
        "state/past/velocity_x": tfrec.FixedLenFeature([128, 10], tfrec.float32, 0.0),
        "state/past/velocity_y": tfrec.FixedLenFeature([128, 10], tfrec.float32, 0.0),
        "state/past/width": tfrec.FixedLenFeature([128, 10], tfrec.float32, 0.0),
        "state/past/x": tfrec.FixedLenFeature([128, 10], tfrec.float32, 0.0),
        "state/past/y": tfrec.FixedLenFeature([128, 10], tfrec.float32, 0.0),
        "state/past/z": tfrec.FixedLenFeature([128, 10], tfrec.float32, 0.0),
    }
    # fmt: on

    # Features of traffic lights.
    traffic_light_features = {}
    traffic_light_types = {
        "state": tfrec.int64,
        "x": tfrec.float32,
        "y": tfrec.float32,
        "z": tfrec.float32,
        "valid": tfrec.int64,
        "timestamp_micros": tfrec.int64,
        "id": tfrec.int64,
    }
    for key, dtype in traffic_light_types.items():
        for prefix, count in [("current", 1), ("past", 10), ("future", 80)]:
            traffic_light_features[
                f"traffic_light_state/{prefix}/{key}"
            ] = tfrec.FixedLenFeature(
                [count, 16], dtype, 0 if dtype == tfrec.int64 else 0.0
            )

    features_description: dict[str, tfrec.FixedLenFeature] = {}
    features_description.update(roadgraph_features)
    features_description.update(state_features)
    features_description.update(traffic_light_features)
    features_description["scenario/id"] = tfrec.FixedLenFeature([], tfrec.string, "")

    records = sorted(record_path.glob("*.tfrecord*"))
    record_idxs = get_tfrecord_cache(record_path, [r.name for r in records])
    tfrecord: dict[str, DataNode] = fn.readers.tfrecord(
        path=list(map(str, records)),
        index_path=record_idxs,
        features=features_description,
        name="womd-reader",
    )

    extra_keys = ["state/tracks_to_predict", "state/is_sdc", "state/id", "state/type"]
    expand_feature_over_sequence(tfrecord, extra_keys)

    concatenate_time(tfrecord)
    state_key_order = (
        "is_sdc",
        "id",
        "type",
        "bbox_yaw",
        "height",
        "length",
        "width",
        "vel_yaw",
        "velocity_x",
        "velocity_y",
        "x",
        "y",
        "z",
        "tracks_to_predict",
        "timestamp_micros",
        "valid",
    )
    transpose_key(tfrecord, "state", perm=[1, 0])  # Transpose to time first

    for key in tfrecord:
        tfrecord[key] = fn.cast(tfrecord[key], dtype=DALIDataType.FLOAT)

    signal_key_order = ("x", "y", "z", "state", "id", "timestamp_micros", "valid")
    roadgraph_order = ("id", "type", "dir", "xyz", "valid")

    for prefix, keys in (
        ("state", state_key_order),
        ("traffic_light_state", signal_key_order),
    ):
        tfrecord[prefix] = stack_features(tfrecord, prefix, keys)

    tfrecord["roadgraph_samples"] = cat_features(
        tfrecord, "roadgraph_samples", roadgraph_order
    )

    return tuple(
        tfrecord[k]
        for k in ["state", "traffic_light_state", "roadgraph_samples", "scenario/id"]
    )
