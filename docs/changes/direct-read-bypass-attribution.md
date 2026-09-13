# Direct-read bypass attribution

Successful reads of files larger than the cacheable threshold are now reported as successful operations with `source=bypass`. Open, pread, and close failures remain negative and preserve errno. This avoids classifying successful backing-filesystem reads as failures.

All reads reject a negative `off_t` with `EINVAL` after starting and before completing the read metric, and before cache lookup, conversion to `size_t`, pointer arithmetic, or backing-file I/O. The dependency-light policy test covers zero and both signed extrema, and a production-wiring test enforces metric/accounting and data-access ordering.
