import subprocess
import time
import statistics
import os
import sys

if len(sys.argv) < 2:
    print("Usage: python get_arg.py <iterations>")
    sys.exit(1)

its = int(sys.argv[1])
output_dir = os.environ.get('JOB_OUTPUT_DIR')
scripts_dir = os.environ.get('SCRIPTS_DIR')

if output_dir is None:
    print("output_dir not found")
    sys.exit(1)

print(f"output_dir: {output_dir}")

view_script = 'view_test.py'
target_script = 'target_test.py'
script_stdout = 'script.output'

if os.path.exists(script_stdout):
    os.remove(script_stdout)


def gen_output_folders(folder, iterations):
    for i in range(iterations):
        try:
            final_dir = output_dir + "/" + folder + "/" + str(i + 1)
            os.makedirs(final_dir, exist_ok=False)
        except Exception as e:
            print(f"Failed to create directory '{final_dir}': {e}")

def get_all_metrics(folder, iteration):
    final_dir = output_dir + '/' + folder + '/' + str(iteration)
    command = [scripts_dir + '/' + "filesystem_ioctl"  + '/' + 'get_all_metrics.sh', final_dir]
    try:
        result = subprocess.run(command, capture_output=True, text=True, check=True)
    except subprocess.CalledProcessError as e:
        if e.stderr:
            print(e.stderr)
            sys.exit(1)

def reset_fs():
    command = [scripts_dir + '/' + "filesystem_ioctl"  + '/' + 'reset_fs.sh']
    try:
        result = subprocess.run(command, capture_output=True, text=True, check=True)
    except subprocess.CalledProcessError as e:
        if e.stderr:
            print(e.stderr)
            sys.exit(1)

def run_script(folder, script_name, iterations):
    execution_times = []
    script_output = output_dir + "/" + script_stdout
    print(f'{script_name} - start run')
    with open(script_output, 'a') as f:
        for i in range(iterations):
            iteration = i + 1
            print(f'{script_name} - iteration: {iteration}')
            start_time = time.time()
            subprocess.run(['python3', script_name], stdout=f)
            end_time = time.time()
            execution_times.append(end_time - start_time)

            if folder == "view":
                get_all_metrics(folder, iteration)
                reset_fs()

    total_time = sum(execution_times)
    if iterations >= 2:
        std_dev = statistics.stdev(execution_times)

    print("====================================================")
    print("=                     METRICS                      =")
    print("====================================================")
    print(f'{script_name} - Total time: {total_time}')
    if iterations >= 2:
        print(f'{script_name} - Stddev time: {std_dev}')
    print("====================================================")


gen_output_folders("view", its)

reset_fs()
run_script("view", view_script, its)
run_script("target", target_script, its)
