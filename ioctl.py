import fcntl
import os
import struct

view_dir = (os.environ['VIEW_DIR'])
cur_dir = os.getcwd()
fd = os.open(view_dir + "/file1", os.O_RDWR)
IOCTL_REQUEST_CODE = 0
result = fcntl.ioctl(fd, IOCTL_REQUEST_CODE, 0)
os.close(fd)
