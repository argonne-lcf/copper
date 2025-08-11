import os
import sys

def count_files_by_size(directory):
    size_categories = {
        "Under 1MB": 0,
        "1MB - 10MB": 0,
        "10MB - 100MB": 0,
        "100MB - 1GB": 0,
        "Over 1GB": 0
    }

    for root, _, files in os.walk(directory):
        for file in files:
            file_path = os.path.join(root, file)
            try:
                size = os.path.getsize(file_path)  # File size in bytes
                if size < 1_048_576:  # 1MB
                    size_categories["Under 1MB"] += 1
                elif size < 10_485_760:  # 10MB
                    size_categories["1MB - 10MB"] += 1
                elif size < 104_857_600:  # 100MB
                    size_categories["10MB - 100MB"] += 1
                elif size < 1_073_741_824:  # 1GB
                    size_categories["100MB - 1GB"] += 1
                else:
                    size_categories["Over 1GB"] += 1
            except OSError:
                continue  # Skip inaccessible files

    return size_categories

directory = str(sys.argv[1])  
print(f"directory = {directory}")
result = count_files_by_size(directory)

for category, count in result.items():
    print(f"{category}: {count}")
