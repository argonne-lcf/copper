import subprocess
import time
import os
import sys
import csv

def check_environment_variables():
    required_vars = [
        'TEST_ID', 'HOSTNAME', 'SCRIPTS_DIR',
        'TEST_OUTPUT_DIR', 'TEST_OUTPUT_VIEW_DIR', 'TEST_OUTPUT_TARGET_DIR',
        'TEST_TIME_VIEW_PATH', 'TEST_TIME_TARGET_PATH',
        'VIEW_SCRIPT_PATH', 'TARGET_SCRIPT_PATH', 'WHAT_TO_TEST'
    ]
    for var in required_vars:
        if not os.environ.get(var):
            print(f"Error: Environment variable {var} is not set.")
            sys.exit(1)

def get_all_metrics(folder):
    reset_fs_script = os.path.join(os.environ.get('SCRIPTS_DIR'), 'filesystem_ioctl', 'get_all_metrics.sh')
    command = [reset_fs_script, folder]
    try:
        subprocess.run(command, capture_output=True, text=True, check=True)
    except subprocess.CalledProcessError as e:
        print(f"{os.environ.get('HOSTNAME')}-{os.environ.get('TEST_ID')}: {e.stdout}")
        if e.stderr:
            print(f"{os.environ.get('HOSTNAME')}-{os.environ.get('TEST_ID')}: {e.stderr}")
        sys.exit(1)

def reset_fs():
    reset_fs_script = os.path.join(os.environ.get('SCRIPTS_DIR'), 'filesystem_ioctl', 'get_all_metrics.sh')
    command = [reset_fs_script]
    try:
        result = subprocess.run(command, capture_output=True, text=True, check=True)
    except subprocess.CalledProcessError as e:
        print(e.stdout)
        if e.stderr:
            print(e.stderr)
        sys.exit(1)

def run_script(get_metrics, script_path, output_folder, time_output_path):
    hostname = os.environ.get('HOSTNAME')
    test_id = os.environ.get('TEST_ID')
    print(f'{hostname}-{test_id}: {script_path} - start run')
    script_stdout = 'script.output'
    script_output = os.path.join(output_folder, script_stdout)

    try:
        start_time = time.time()
        print(f"{hostname}-{test_id}: start_time: {start_time}")
        with open(script_output, 'a') as f:
            subprocess.run(['python3', script_path], stdout=f, check=True)
        end_time = time.time()
        print(f"{hostname}-{test_id}: end_time: {end_time}")
        total_time = end_time - start_time
        print(f"{hostname}-{test_id}: total time - {total_time}")

        data = [hostname, test_id, start_time, end_time, total_time]
        with open(time_output_path, "w", newline='') as t_file:
            t_writer = csv.writer(t_file)
            t_writer.writerow(["hostname", "test_id", "start_time", "end_time", "total_time"])
            t_writer.writerow(data)

        if get_metrics:
            get_all_metrics(output_folder)

    except subprocess.CalledProcessError as e:
        print(f"Error running {script_path}: {e}")
        sys.exit(1)
    except Exception as e:
        print(f"Unexpected error: {e}")
        sys.exit(1)

def main():
    check_environment_variables()

    what_to_test = os.environ.get('WHAT_TO_TEST')
    if what_to_test not in ["view_and_target", "view", "target"]:
        print("Invalid value for WHAT_TO_TEST.")
        sys.exit(1)

    scripts_dir = os.environ.get('SCRIPTS_DIR')

    if what_to_test in ["view_and_target", "view"]:
        view_script_path = os.environ.get('VIEW_SCRIPT_PATH')
        view_output_dir = os.environ.get('TEST_OUTPUT_VIEW_DIR')
        test_time_view_path = os.environ.get('TEST_TIME_VIEW_PATH')
        run_script(True, view_script_path, view_output_dir, test_time_view_path)

    if what_to_test in ["view_and_target", "target"]:
        target_script_path = os.environ.get('TARGET_SCRIPT_PATH')
        target_output_dir = os.environ.get('TEST_OUTPUT_TARGET_DIR')
        test_time_target_path = os.environ.get('TEST_TIME_TARGET_PATH')
        run_script(False, target_script_path, target_output_dir, test_time_target_path)

if __name__ == "__main__":
    main()
