# WOMD Conversion Example

This is an example for applying the framework on another complete dataset such as Waymo Open Motion Dataset. We convert their tfrecord dataset into our format by reading the dataset with DALI and converting the yielded tensors to our format. Much like SC2, we observe a significant reduction in dataset size, results are show in the table below.

## Building

Build the WOMD bindings by enabling them when building the main library with `-DSC2_WOMD_EXAMPLE=ON` and symbolically link them in this directory e.g. `ln -s ../build/Release/_womd_binding.cpython-312-x86_64-linux-gnu.so _womd_binding.cpython-312-x86_64-linux-gnu.so`. You can manually update the included `.pyi` stub with `pybind11-stubgen` if there are changes.

## Size Comparison Between TF Record and Our Format for WOMD (GB)

| Format  | Training | Validation | Testing | Total | % diff |
| ------  | -------- | ---------- | ------- | ----- | ------ |
| TF Rec. | 619.4    | 56.1       | 58.3    | 733.8 | -      |
| Ours    | 145.5    | 13.1       | 11.3    | 169.9 | -76    |

The roadgraph data is the most significant contribution of the WOMD dataset's serialized size, as opposed to Starcraft II that is mostly agent data. 10.81 GB of the 13.13GB of the validation dataset in our format is roadgraph data which was tested by overwriting `sequence.agentData` and `sequence.signalData` to an empty list in the `convert` command. If a new dataset was to be curated, a significantly longer time duration per example could be used with a lower proportional cost to increasing the serialized size of the dataset.
