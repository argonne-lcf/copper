import fcntl
import os
import struct

cur_dir = os.getcwd();
fd = os.open(cur_dir + "/" + "view" + "/file1", os.O_RDWR)
IOCTL_REQUEST_CODE = 0
result = fcntl.ioctl(fd, IOCTL_REQUEST_CODE, 0)
os.close(fd)
