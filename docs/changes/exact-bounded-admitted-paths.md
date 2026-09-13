# Exact bounded admitted-set path accounting

Top-N mode now admits the first N distinct `(source,path)` keys for an epoch and keeps their attribution exact. Events for any unadmitted key increment only `other_events`; records are never replaced and counts are never transferred between paths. Therefore each retained row's `total_events` equals its outcome plus TTL-event fields, and `sum(retained total_events) + other_events == paths_seen`. This is bounded and exact for the admitted set, but is not a frequency heavy-hitter algorithm.

Regression coverage includes adversarial new-key arrivals and accounting invariants.
