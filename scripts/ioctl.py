import fcntl
import os
import sys

spec_dev = (os.environ['SPEC_DEV_VIEW'])
cur_dir = os.getcwd()
ioctl_request_code = sys.argv[1]

if spec_dev is None:
    print("spec_dev not found")
if ioctl_request_code is None:
    print("ioctl_request_code not found")

fd = os.open(spec_dev, os.O_RDWR)
IOCTL_REQUEST_CODE = int(ioctl_request_code)
result = fcntl.ioctl(fd, IOCTL_REQUEST_CODE, 0)
os.close(fd)
