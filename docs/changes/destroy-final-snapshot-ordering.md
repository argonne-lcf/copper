# Destroy included in final metrics

The destroy callback now records its successful completion after engine finalization and before the final profiling snapshot and reset. Consequently the final bundle includes destroy latency/count and no destroy event is stranded in a newly reset epoch. The earlier pre-destroy diagnostic snapshot remains intentionally prior to teardown.
