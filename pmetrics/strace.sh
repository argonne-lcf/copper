strace -s 0 -e trace=read,open,openat -o strace.output python3 test.py
