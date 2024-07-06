import sys 
import os

print("view_test script")

view_dir = os.environ['VIEW_DIR']
packages_dir = os.environ['PY_PACKAGES_DIR']

packages = view_dir + packages_dir
print(f"packages dir: {packages}")
sys.path.insert(0, packages)

import numpy
print(numpy.__file__)
