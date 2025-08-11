import os
import sys

def get_depth_paths(directory):
    min_depth = float('inf')
    max_depth = 0
    min_depth_path = ""
    max_depth_path = ""

    for root, _, _ in os.walk(directory):
        depth = root.count(os.sep) - directory.count(os.sep) + 1

        # Check for minimum depth
        if depth < min_depth:
            min_depth = depth
            min_depth_path = root

        # Check for maximum depth
        if depth > max_depth:
            max_depth = depth
            max_depth_path = root

    return min_depth, min_depth_path, max_depth, max_depth_path

# Example usage

directory = str(sys.argv[1])  
print(f"directory = {directory}")
min_depth, min_depth_path, max_depth, max_depth_path = get_depth_paths(directory)

print(f"Shallowest path (Depth {min_depth}): {min_depth_path}")
print(f"Deepest path (Depth {max_depth}): {max_depth_path}")
