import sys

def main(f):
    lines = ["", ""]
    cur_line = 0
    for line in open(f, "r"):
        lines[cur_line % 2] = line.strip()

        if cur_line % 2 == 0:
            host = lines[0].split(".")[0]
            addr = lines[1]

            if host[0] != 'x':
                continue

            print(f"getent hosts {host}")

        cur_line += 1

if __name__ == "__main__":
    if len(sys.argv) < 2:
        printf("usage: gen_getent_host <filepath>")
    
    main(sys.argv[1])
