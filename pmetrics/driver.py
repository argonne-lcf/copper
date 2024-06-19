import subprocess
import time
import statistics
import os

view_script = 'view_test.py'
target_script = 'target_test.py'
script_stdout = 'script.output'

if os.path.exists(script_stdout):
    os.remove(script_stdout)

def run_script(script_name, iterations):
    execution_times = []

    print(f'{script_name} - start run')
    with open(script_stdout, 'a') as f:
        for i in range(iterations):
            iteration = i + 1
            print(f'{script_name} - iteration: {iteration}')
            start_time = time.time()
            subprocess.run(['python3', script_name], stdout=f)
            end_time = time.time()
            execution_times.append(end_time - start_time)

    total_time = sum(execution_times)
    std_dev = statistics.stdev(execution_times)
    
    print("====================================================")
    print("=                     METRICS                      =")
    print("====================================================")
    print(f'{script_name} - Total time: {total_time}')
    print(f'{script_name} - Stddev time: {std_dev}')
    print("====================================================")

run_script(view_script, 10)
run_script(target_script, 10)
