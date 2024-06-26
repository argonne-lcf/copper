import sys 
import os

view_dir = os.environ['TARGET_DIR']
packages = view_dir + "/" + "packages"

sys.path.insert(0, packages)

import numpy
print(numpy.__file__)
