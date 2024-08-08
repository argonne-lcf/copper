import fcntl
import os
import sys

ioctl_request_code = sys.argv[1]
ioctl_dev = sys.argv[2]

if ioctl_dev is None:
    print("ioctl_dev not found")
if ioctl_request_code is None:
    print("ioctl_request_code not found")

print(f"opening file: {ioctl_dev}")

fd = os.open(ioctl_dev, os.O_RDWR)
try:
    print(f"ioctl with cmd: {ioctl_request_code}")
    result = fcntl.ioctl(fd, int(ioctl_request_code), 0)
    print("ioctl operation successful")
except Exception as e:
    print(f"ioctl operation failed: {e}")
finally:
    os.close(fd)
