import sys 
import os

view_dir = os.environ['VIEW_DIR']
packages = view_dir + "/" + "packages"

sys.path.insert(0, packages)

import torch 
print(torch.__file__)
