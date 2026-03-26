import sys
import statx

def pprint_dict(d):
    output = ""
    for key, value in d.items():
        output += f"    {key}: {value}\n"
    sys.stdout.write("statx: \n")
    sys.stdout.write(output)
    sys.stdout.flush()

def get_file_info(path):
    try:
        stx = statx.statx(path)

        file_info = {
            'nlink': stx.nlink,
            'dev': stx.dev,
            'rdev': stx.rdev,
            'uid': stx.uid,
            'gid': stx.gid,
            'filetype': stx.filetype,
            'mode': stx.mode,
            'ino': stx.ino,
            'size': stx.size,
            'blocks': stx.blocks,
            'atime': stx.atime,
            'btime': stx.btime,
            'ctime': stx.ctime,
            'mtime': stx.mtime
        }

        pprint_dict(file_info)

    except FileNotFoundError:
        print(f"Error: {path} not found")
    except PermissionError:
        print(f"Error: Permission denied to access {path}")
    except OSError as e:
        print(f"Error: {e}")
    except Exception as e:
        print(f"Error: {e}")

if __name__ == "__main__":
    if len(sys.argv) != 2:
        print("Usage: python3 statx_example.py <filename>")
        sys.exit(1)

    path = sys.argv[1]
    get_file_info(path)
