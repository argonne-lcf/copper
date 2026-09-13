# Manifest-committed aggregation

The offline aggregator discovers only manifests and consumes a bundle only when it declares exactly one aggregate, operations, and top-path CSV, all exist, and every declared basename is derived from that manifest's stem. Duplicate, extra, malformed, and cross-stem entries are rejected. A file or physical bundle referenced by multiple manifests is rejected globally rather than counted twice. Orphan and partial CSVs and malformed manifests are ignored. Mixed committed schema versions and malformed numeric fields fail clearly instead of producing plausible partial totals.

Schema-v2 count and nanosecond fields are parsed and accumulated as Python integers. Their exact values are retained even above `2^53`; floating point is used only for the derived displayed average latency.

A manifest is the commit marker because report writers publish it only after all CSV renames succeed.
