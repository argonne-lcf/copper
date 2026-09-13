# Backing permission access checks

Read and existence checks now use `faccessat` against the translated backing path with effective-ID semantics and no-follow where supported, preserving the syscall errno. Requests containing `W_OK` return `EROFS` because Copper is a read-only overlay, regardless of backing-file writability.
