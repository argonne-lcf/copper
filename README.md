# Co-operative Caching Layer for Copper ​

As the number of nodes grows large (i.e. > 512) redundant IO requests for the same 
resources can overwhelm the underlying storage system causing extremely high latency. 
This project aims to intercept and cache shared IO operations in distributed node memory.
This elinates all nodes reaching the underlying storage system for a resource needed by
all nodes. 

## Notes from week 3 meeting

Reading should never mutate the cache. Instead, on filesystem mount the target dir is
recursively read through and stored in the cache. From then on, reading and writing only
affects the cache instead of reaching the underlying/passthrough filesystem.

## Metadata Cache Table Progress

Lists of the current filesystem operations for which the caching is implemented.

- [x] getattr | DOES NOT CHECK PERMISSIONS
- [ ] readlink
- [ ] mknod
- [ ] mkdir
- [x] unlink
- [ ] rmdir
- [ ] symlink
- [ ] rename
- [ ] link
- [ ] chmod
- [ ] chown
- [ ] truncate
- [x] open
- [x] read
- [ ] write
- [ ] statfs
- [ ] release
- [ ] fsync
- [ ] readdir
- [ ] init
- [ ] access
- [ ] create
- [ ] utimens
- [ ] lseek
