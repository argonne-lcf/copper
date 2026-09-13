# Exact latency bounds with bounded reservoir quantiles

Operation count, cumulative latency, epoch minimum, and epoch maximum are exact. Percentiles are computed from a fixed-capacity Algorithm R reservoir and are estimates after the count exceeds capacity. Reports expose `sample_count`, `latency_capacity`, and `sampling_method=reservoir_algorithm_r` rather than describing the sample as the complete epoch.

Regression coverage records more observations than capacity and verifies exact count/min/max/cumulative values and bounded sample size.
