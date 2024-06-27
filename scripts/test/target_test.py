import sys
import os

print("target_test script")

packages_dir = os.environ['PY_PACKAGES_DIR']

print(f"packages dir: {packages_dir}")

sys.path.insert(0, packages_dir)

import numpy
print(numpy.__file__)
