import sys

def main(f1, f2):
    ip_to_host = {}
    for line in open(f1, "r"):
        line_split = line.split()
        ip_to_host[line_split[0]] = line_split[1].split('.')[0]

    for line in open(f2, "r"):
        line_split = line.split()
        ip = line_split[2]
        mac = line_split[6].split(":")

        if ip in ip_to_host:
            print(f"{ip_to_host[ip].split('-')[0]} ofi+cxi:0x2{mac[-3][1]}{mac[-2]}{mac[-1]}0")
        
        
if __name__ == "__main__":
    if len(sys.argv) < 3:
        print("usage: gen_address_book.py <filepath1> <filepath2>")
    
    main(sys.argv[1], sys.argv[2])
