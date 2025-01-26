# WOMD Conversion Example

This is an example for applying the framework on another complete dataset such as Waymo Open Motion Dataset. We convert their tfrecord dataset into our format by reading the dataset with DALI and converting the yielded tensors to our format. Much like SC2, we observe a significant reduction in dataset size, results are show in the table below.

### Size Comparison Between TF Record and Our Format for WOMD (GB)

| Format  | Training | Validation | Testing | Total | % diff |
| ------  | -------- | ---------- | ------- | ----- | ------ |
| TF Rec. | 619.4    | 56.1       | 58.3    | 733.8 | -      |
| Ours    | 76.1     | 6.9        | 7.0     | 90.0  | -87    |
