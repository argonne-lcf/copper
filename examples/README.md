# Copper Examples

Platform-specific example scripts are organized under:

- `aurora_examples/`
- `frontier_examples/`

Each directory contains:

- `launch_copper_<platform>.sh`
- `stop_copper_<platform>.sh`
- `simple_with_copper.sh`
- `simple_without_copper.sh`

These examples are intended to show common module loads, mount paths, and job
launch commands for each site. The Aurora and Frontier directories contain
standalone platform-specific launch and stop wrappers so each example can be
read and adapted in isolation, even where that means some intentional code
duplication. They are starting points for local adaptation rather than drop-in
production job scripts.

After build, the wrappers default to the staged facility address-book files in
`../build/` through `${COPPER_ROOT}/build/...`:

- `build/olcf_frontier_copper_addressbook.txt`
- `build/alcf_aurora_copper_addressbook.txt`

The source copies remain under `scripts/` and are copied into `build/` by the
CMake post-build step.
