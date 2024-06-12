# Co-operative Caching Layer for Copper ​

As the number of nodes grows large (i.e. > 512) redundant IO requests for the same 
resources can overwhelm the underlying storage system causing extremely high latency. 
This project aims to intercept and cache shared IO operations in distributed node memory.
This elinates all nodes reaching the underlying storage system for a resource needed by
all nodes. 

## Metadata Cache Table Progress

Lists of the current filesystem operations for which the metadata cache table is 
implemented.

- [x] getattr | DOES NOT CHECK PERMISSIONS
- [ ] readlink
- [ ] mknod
- [ ] mkdir
- [ ] unlink
- [ ] rmdir
- [ ] symlink
- [ ] rename
- [ ] link
- [ ] chmod
- [ ] chown
- [ ] truncate
- [ ] open
- [ ] read
- [ ] write
- [ ] statfs
- [ ] flush | MAY NOT SUPPORT
- [ ] release
- [ ] fsync
- [ ] setxattr
- [ ] getxattr
- [ ] listxattr
- [ ] removexattr
- [ ] opendir | MAY NOT SUPPORT
- [ ] readdir
- [ ] releasedir | MAY NOT SUPPORT
- [ ] fsyncdir | MAY NOT SUPPORT
- [ ] init
- [ ] destroy | MAY NOT SUPPORT
- [ ] access
- [ ] create
- [ ] lock | MAY NOT SUPPORT
- [ ] utimens
- [ ] bmap | MAY NOT SUPPORT
- [ ] ioctl | MAY NOT SUPPORT
- [ ] poll | MAY NOT SUPPORT
- [ ] write_buf | MAY NOT SUPPORT
- [ ] read_buf | MAY NOT SUPPORT
- [ ] flock | MAY NOT SUPPORT
- [ ] fallocate
- [ ] copy_file_range
- [ ] lseek

### Data Cache Table Progress

Lists of the current filesystem operations for which the metadata cache table is implemented.

- [ ] getattr
- [ ] readlink
- [ ] mknod
- [ ] mkdir
- [ ] unlink
- [ ] rmdir
- [ ] symlink
- [ ] rename
- [ ] link
- [ ] chmod
- [ ] chown
- [ ] truncate
- [ ] open
- [x] read | DOES NOT UPDATE METADATA
- [ ] write
- [ ] statfs
- [ ] flush | MAY NOT SUPPORT
- [ ] release
- [ ] fsync
- [ ] setxattr
- [ ] getxattr
- [ ] listxattr
- [ ] removexattr
- [ ] opendir | MAY NOT SUPPORT
- [ ] readdir
- [ ] releasedir | MAY NOT SUPPORT
- [ ] fsyncdir | MAY NOT SUPPORT
- [ ] init
- [ ] destroy | MAY NOT SUPPORT
- [ ] access
- [ ] create
- [ ] lock | MAY NOT SUPPORT
- [ ] utimens
- [ ] bmap | MAY NOT SUPPORT
- [ ] ioctl | MAY NOT SUPPORT
- [ ] poll | MAY NOT SUPPORT
- [ ] write_buf | MAY NOT SUPPORT
- [ ] read_buf | MAY NOT SUPPORT
- [ ] flock | MAY NOT SUPPORT
- [ ] fallocate
- [ ] copy_file_range
- [ ] lseek
