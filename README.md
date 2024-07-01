# Co-operative Caching Layer for Copper ​

### Lists of the current filesystem operations that are supported.

- [x] destroy
- [x] init
- [x] ioctl
- [x] readdir
- [x] read
- [x] open
- [x] getattr
- [x] readlink

### How to build

In order to build we recommend installing mochi dependencies using spack and their environment feature. You can find the instructions to install spack [here](https://spack-tutorial.readthedocs.io/en/latest/tutorial_basics.html). The required mochi services are margo, mercury, thallium, and cereal. The instructions to install the listed mochi services can be found [here](https://mochi.readthedocs.io/en/latest/installing.html).

Assuming you have a mochi environment setup correctly you should now be able to build by running the following commands.

#### Clone the git repository
```
git clone https://github.com/argonne-lcf/copper <COPPER_BASE>
```

#### Set the `env.sh`
```
cd <COPPER_BASE>/scripts
cp default_env.sh env.sh
```

#### Set the following variables in the copied `env.sh`
```
export VIEW_DIR=<PATH_TO_MOUNT_DIRECTORY>
export FUSE3_LIB=<PATH_TO_FUSE_LIBRARY>
export FUSE3_INCLUDE=<PATH_TO_FUSE_HEADER>
```

#### Activate spack environment and build
```
spack env activate <MOCHI_ENV>
sh <COPPER_BASE>/scripts/build_helper/build.sh
```

