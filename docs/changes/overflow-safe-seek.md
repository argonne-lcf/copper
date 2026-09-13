# Overflow-safe seek policy

`SEEK_SET` and `SEEK_END` support any offset that produces a representable, non-negative result; positive `SEEK_END` offsets are valid. Overflow returns `EOVERFLOW`, and positions before zero return `EINVAL`. `SEEK_CUR` remains `ENOSYS` because callbacks do not own per-handle offset state.

The negative-side guard compares `off < -size`, where `size` is already non-negative, rather than negating `off`; this remains defined for `off_t` minimum. UBSan regression coverage exercises both `off_t` extrema and positive overflow.
