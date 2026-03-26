#!/bin/bash -x
# Full-path profiling validation:
# - enables profiling
# - keeps the hottest 20 paths
# - also enables full-path forensic output
# sbatch ./profiling_full_path_list_jobscript.sh

#SBATCH -A UMS046
#SBATCH -J copper_prof_full
#SBATCH -o %x-%j.out
#SBATCH -t 00:15:00
#SBATCH -p batch
#SBATCH -N 512
#SBATCH -C nvme

cd "$SLURM_SUBMIT_DIR"
echo "$SLURM_JOBID"
echo "$SLURM_JOB_NUM_NODES"
echo "$SLURM_JOB_NAME"
echo "$SLURM_NODELIST"

PACKAGE_ENV_LOCATION=${PACKAGE_ENV_LOCATION:-/path/to/benchmark_builds}
if [ -f "${PACKAGE_ENV_LOCATION}/common_modules.sh" ]; then
  source "${PACKAGE_ENV_LOCATION}/common_modules.sh"
fi
module load ums ums046 copper
module list

BASE=/lustre/orion/proj-shared/ums046/final-exp-test/test4_profiling_full_path_list
COPPER_DEV_ROOT=/lustre/orion/proj-shared/ums046/copper-changes/version9-production/copper
export COPPER_ROOT="${COPPER_DEV_ROOT}"
COPPER_LAUNCH="${COPPER_DEV_ROOT}/scripts/launch_copper.sh"
COPPER_STOP="${COPPER_DEV_ROOT}/scripts/stop_copper.sh"
COPPER_MOUNT_PATH=/mnt/bb/$USER/copper_mount

cd "${BASE}"
LOGDIR="${BASE}"

echo "batch_pwd=$(pwd)"
echo "COPPER_ROOT=${COPPER_ROOT}"
echo "COPPER_LAUNCH=${COPPER_LAUNCH}"
echo "COPPER_STOP=${COPPER_STOP}"
ls -l "${COPPER_DEV_ROOT}/build/cu_fuse"

if ! "${COPPER_LAUNCH}" -d "${LOGDIR}" -v "${COPPER_MOUNT_PATH}" -s 10 -b "1,2" -l 1 -E 2000 -P -N 20 -A; then
  echo "Copper launch failed; aborting before conda/import phase"
  exit 1
fi

JOB_OUT_DIR="${LOGDIR}/${SLURM_JOB_ID}"
JOB_LOG_DIR="${JOB_OUT_DIR}/logs"
NODEFILE="${JOB_LOG_DIR}/node_file.txt"
SLURM_STDOUT_FILE="${SLURM_JOB_NAME}-${SLURM_JOB_ID}.out"

search_rank_logs() {
  local pattern=$1
  local outfile=$2
  : > "${outfile}"
  if [ -d "${JOB_LOG_DIR}" ]; then
    find "${JOB_LOG_DIR}" -maxdepth 1 -type f -name '*-output.log' -print0 | \
      xargs -0 -r grep -nH -F -- "${pattern}" > "${outfile}" || true
  fi
}

CPU_BINDING_MAP=verbose,map_cpu:9,17,25,33,41,49,57,73
if ! conda activate "${COPPER_MOUNT_PATH}/${PACKAGE_ENV_LOCATION}/python-pytorch/conda_env"; then
  echo "conda activate failed; aborting before import phase"
  exit 1
fi
/usr/bin/time srun --export=ALL --overlap -N "${SLURM_NNODES}" -n $((SLURM_NNODES * 8)) --ntasks-per-node=8 --cpu-bind="${CPU_BINDING_MAP}" python3 -c "import torch"

if [ -f "${NODEFILE}" ]; then
  "${COPPER_STOP}" -d "${LOGDIR}" -v "${COPPER_MOUNT_PATH}" || true
fi

set +x
find "${JOB_LOG_DIR}" -maxdepth 1 -type f -name '*-output.log' 2>/dev/null | wc -l > "${JOB_OUT_DIR}/output-log-count.txt"
find "${JOB_LOG_DIR}" -maxdepth 1 -type f -name '*-output.log' -size +0c 2>/dev/null | wc -l > "${JOB_OUT_DIR}/nonempty-output-log-count.txt"
: > "${JOB_OUT_DIR}/slurm-prepared-address-book-lines.txt"
if [ -f "${SLURM_STDOUT_FILE}" ]; then
  grep -n "prepared address book generation took" "${SLURM_STDOUT_FILE}" > "${JOB_OUT_DIR}/slurm-prepared-address-book-lines.txt" || true
fi
search_rank_logs "HG_NOENTRY" "${JOB_OUT_DIR}/slurm-hg-noentry-lines.txt"
search_rank_logs "provider registration completed after" "${JOB_OUT_DIR}/slurm-provider-ready-lines.txt"
search_rank_logs "first successful parent rpc_" "${JOB_OUT_DIR}/slurm-first-successful-parent-rpc-lines.txt"
search_rank_logs "parent readiness confirmed" "${JOB_OUT_DIR}/slurm-parent-readiness-confirmed-lines.txt"
python3 "${COPPER_DEV_ROOT}/scripts/aggregate_profiling.py" "${JOB_OUT_DIR}" cluster > "${JOB_OUT_DIR}/profiling-cluster-aggregation.log" 2>&1 || true
find "${JOB_OUT_DIR}/profiling" -type f | sort > "${JOB_OUT_DIR}/profiling-file-list.txt" || true
find "${JOB_OUT_DIR}/profiling/cluster" -maxdepth 1 -type f -name '*-profiling_top_paths.csv' | sort > "${JOB_OUT_DIR}/cluster-top-path-files.txt" || true
find "${JOB_OUT_DIR}/profiling/final" -maxdepth 1 -type f -name '*-profiling_top_paths.csv' | sort > "${JOB_OUT_DIR}/final-top-path-files.txt" || true
find "${JOB_OUT_DIR}/profiling/pre-destroy" -maxdepth 1 -type f -name '*-profiling_top_paths.csv' | sort > "${JOB_OUT_DIR}/predestroy-top-path-files.txt" || true
find "${JOB_OUT_DIR}/profiling/cluster" -maxdepth 1 -type f -name '*-profiling_summary.md' | sort > "${JOB_OUT_DIR}/cluster-summary-files.txt" || true
find "${JOB_OUT_DIR}/profiling/cluster" -maxdepth 1 -type f -name '*-profiling_aggregate.csv' | sort > "${JOB_OUT_DIR}/cluster-aggregate-files.txt" || true
find "${JOB_OUT_DIR}/tables" -type f -name '*_table_cache_event.output' | sort > "${JOB_OUT_DIR}/full-path-artifact-files.txt" || true
set -x
