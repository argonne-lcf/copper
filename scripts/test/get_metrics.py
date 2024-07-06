import csv
import os
import glob
import sys
import numpy as np
import pandas as pd

def find_csv_files(search_directory, pattern):
    return glob.glob(os.path.join(search_directory, pattern), recursive=True)

def check_env_variable(variable_name):
    value = os.environ.get(variable_name)
    if not value:
        print(f"{variable_name} not found in environment variables.")
        sys.exit(1)
    return value

def parse_csv(file_path, csv_output_path, csv_header, header_map):
    if not os.path.exists(file_path):
        print(f"File {file_path} not found. Skipping...")
        return

    with open(file_path, mode='r') as file:
        reader = csv.reader(file)
        header = next(reader)
        print(f"Header: {csv_header}")
        if csv_output_path not in header_map:
            header_map[csv_output_path] = False  # Initialize header flag for this output file

        with open(csv_output_path, "a", newline='') as t_file:
            t_writer = csv.writer(t_file)
            if not header_map[csv_output_path]:
                t_writer.writerow(csv_header)
                header_map[csv_output_path] = True  # Set header flag to True after writing once

            for row in reader:
                print(f"Row: {row}")
                t_writer.writerow(row)

def calculate_statistics(csv_file, top_perc, bot_perc):
    if not os.path.exists(csv_file):
        print(f"File {csv_file} not found. Skipping metrics calculation.")
        return

    # Read CSV into a pandas DataFrame
    df = pd.read_csv(csv_file)

    # Convert 'total_time' column to numeric (in case it's not already)
    df['total_time'] = pd.to_numeric(df['total_time'], errors='coerce')

    # Calculate statistics
    mean = df['total_time'].mean()
    std_dev = df['total_time'].std()
    top_perc_res = np.percentile(df['total_time'], 100 - top_perc)
    bot_perc_res = np.percentile(df['total_time'], bot_perc)

    # Print statistics
    print(f"mean total_time: {mean}")
    print(f"standard deviation total_time: {std_dev}")
    print(f"largest {top_perc}% of total_time: {top_perc_res}")
    print(f"smallest {bot_perc}% of total_time: {bot_perc_res}")

def main(search_directory):
    if len(sys.argv) != 2:
        print("Usage: get_metrics.py <directory>")
        sys.exit(1)

    # Get environment variables
    view_csv_output_path = check_env_variable('CSV_VIEW_OUTPUT_PATH')
    target_csv_output_path = check_env_variable('CSV_TARGET_OUTPUT_PATH')

    # Define CSV header
    csv_header = ["hostname", "test_id", "start_time", "end_time", "total_time"]

    # Find CSV files
    target_csv_files = find_csv_files(search_directory, '**/target/**/*.csv')
    view_csv_files = find_csv_files(search_directory, '**/view/**/*.csv')

    # Print found CSV files
    print("Target CSV files found:")
    for csv_file in target_csv_files:
        print(csv_file)
    print("\nView CSV files found:")
    for csv_file in view_csv_files:
        print(csv_file)

    # Dictionary to track header writing per output path
    header_map = {}

    # Parse CSV files
    print("\nParsing view CSV files:")
    for csv_file in view_csv_files:
        print(f"Parsing file: {csv_file}")
        parse_csv(csv_file, view_csv_output_path, csv_header, header_map)

    print("\nParsing target CSV files:")
    for csv_file in target_csv_files:
        print(f"Parsing file: {csv_file}")
        parse_csv(csv_file, target_csv_output_path, csv_header, header_map)

    # Calculate statistics
    print(f"\nGenerating statistics for: {target_csv_output_path}")
    print("======================= TARGET METRICS =======================")
    calculate_statistics(target_csv_output_path, 5, 5)

    print(f"\nGenerating statistics for: {view_csv_output_path}")
    print("======================= VIEW METRICS =======================")
    calculate_statistics(view_csv_output_path, 5, 5)

    print("===============================================================")

if __name__ == "__main__":
    main(sys.argv[1])
