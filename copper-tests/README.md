# Copper Tests

This directory keeps small standalone Mochi/Thallium experiments that are
useful when validating pieces of the Copper runtime without launching the full
filesystem stack.

## Main Areas

- `test1/`
  - one-RPC request/response experiments
  - simple remote shutdown helpers
- `test2/`
  - tree-style forwarding experiment
- `test3-rpc/`
  - minimal client/server RPC latency example
- `test4-rdma/`
  - minimal bulk RDMA example
- `copper-get-addressbook/`
  - local helper used by Copper discover-address mode

## Build

Load the site compiler/MPI/Mochi environment first, then run:

```bash
cd /lustre/orion/proj-shared/ums046/copper-changes/copper/copper-tests
./build.sh
```

Optional environment overrides:

```bash
BUILD_TYPE=Debug ./build.sh
BUILD_DIR=/tmp/copper-tests-build ./build.sh
BUILD_JOBS=8 ./build.sh
```

## Important Binaries

After build, these are the main expected artifacts:

- `build/copper_special_single_rpc`
- `build/shutdown`
- `build/shutdown_special`
- `build/copper`
- `build/client_rpc`
- `build/server_rpc`
- `build/client_bulk_rdma`
- `build/server_bulk_rdma`
- `build/list_cxi_hsn_thallium`

For the full Copper runtime tree, the top-level build also stages the shipped
facility address-book files in:

- `../build/olcf_frontier_copper_addressbook.txt`
- `../build/alcf_aurora_copper_addressbook.txt`

The runtime launch wrappers default to those build-staged copies.

## Address-Book Helper Commands

Build just the helper:

```bash
cd /lustre/orion/proj-shared/ums046/copper-changes/copper/copper-tests/copper-get-addressbook
./compile_thallium_addressbook.sh
```

Frontier/Slurm example:

```bash
srun -N "${SLURM_NNODES}" --ntasks-per-node=1 ../build/list_cxi_hsn_thallium
```

Aurora/PBS example:

```bash
qsub run_thallium_addressbook.pbs
```
