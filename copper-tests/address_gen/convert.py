import re

# Define mappings
s_mapping = {
    0: '66',
    1: '46',
    2: '62',
    3: '42',
    4: '20',
    5: '00',
    6: '24',
    7: '04'
}

c_mapping = {
    0: '1',
    1: '3',
    2: '5',
    3: '7',
    4: '9',
    5: 'b',
    6: 'd',
    7: 'f'
}

# Step 1: Read and collect unique x_values from file in order
input_lines = []
unique_x_values = []

with open("input.txt", "r") as file:
    for line in file:
        line = line.strip()
        input_lines.append(line)
        match = re.match(r"x(\d+)c(\d+)s(\d+)", line)
        if match:
            x_val = int(match.group(1))
            if x_val not in unique_x_values:
                unique_x_values.append(x_val)

# Step 2: Map x_values to sequential hex starting from 09
x_mapping = {}
start_hex = 9
for i, x in enumerate(unique_x_values):
    x_mapping[x] = format(start_hex + i, '02x')

# Step 3: Process each line again with correct mappings
for line in input_lines:
    match = re.match(r"x(\d+)c(\d+)s(\d+)", line)
    if match:
        x_val = int(match.group(1))
        c_val = int(match.group(2))
        s_val = int(match.group(3))

        mapped_x = x_mapping.get(x_val, "??")
        mapped_c = c_mapping.get(c_val, "??")
        mapped_s = s_mapping.get(s_val, "??")

        print(f"{line} ofi+cxi://0x0{mapped_x}{mapped_c}{mapped_s}00")
    else:
        print(f"Invalid format: {line}")
