# Address-Book Discovery Helper

This directory contains the local `version9-production` copy of the
`list_cxi_hsn_thallium` helper and the small scripts used to build or run it.

## Files

- `list_cxi_hsn_thallium.cpp`
  - enumerates `hsn*` interfaces
  - records IPv4 and MAC data
  - tries to create Thallium CXI endpoints and prints them in a table
- `compile_thallium_addressbook.sh`
  - builds only the helper through the local `copper-tests` CMake tree
- `run_thallium_addressbook.pbs`
  - PBS example that builds if needed and runs one rank per node
- `run_thallium_addressbook.slurm`
  - Slurm example that builds if needed and runs one rank per node

## Build

After loading the site MPI/Mochi toolchain:

```bash
cd /lustre/orion/proj-shared/ums046/copper-changes/version9-production/copper/copper-tests/copper-get-addressbook
./compile_thallium_addressbook.sh
```

## Run

Slurm:

```bash
sbatch run_thallium_addressbook.slurm
```

PBS:

```bash
qsub run_thallium_addressbook.pbs
```

The raw helper output is a pipe-delimited table that can be post-processed
into Copper's simpler `hostname endpoint` format.
