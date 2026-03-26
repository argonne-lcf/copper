#!/bin/bash -x
CU_FUSE_MNT_VIEWDIR=/tmp/${USER}/copper
log_dir=~/copper-logs

while getopts "d:v:" opt; do
  case ${opt} in
    d ) log_dir=$OPTARG ;;
    v ) CU_FUSE_MNT_VIEWDIR=$OPTARG ;;
    \? ) echo "Usage: cmd [-d log_dir] [-v CU_FUSE_MNT_VIEWDIR] ; log_dir - pass the path to the node_file.txt under the copper logs dir used in launch copper" ; exit 1 ;;
  esac
done

log_dir=${log_dir%/}/${SLURM_JOB_ID}
SLURM_NODEFILE=${log_dir}/node_file.txt
cleanup_helper=${log_dir}/stop_copper_remote.sh
mkdir -p "$(dirname "${cleanup_helper}")"

cat > "${cleanup_helper}" <<'EOF'
#!/bin/bash
set +e
ulimit -c 0

mnt="$1"

mount_is_active() {
  if command -v findmnt >/dev/null 2>&1; then
    findmnt -T "${mnt}" >/dev/null 2>&1
  elif command -v mountpoint >/dev/null 2>&1; then
    mountpoint -q "${mnt}" >/dev/null 2>&1
  else
    grep -qs " ${mnt} " /proc/mounts
  fi
}

graceful_unmount() {
  local tries=0
  while mount_is_active && [ "${tries}" -lt 5 ]; do
    fusermount3 -u "${mnt}" >/dev/null 2>&1 || true
    mount_is_active || break
    sleep 2
    tries=$((tries + 1))
  done

  if mount_is_active; then
    fusermount3 -uz "${mnt}" >/dev/null 2>&1 || true
  fi
}

graceful_unmount

if pgrep -x cu_fuse >/dev/null 2>&1; then
  pkill -TERM -x cu_fuse >/dev/null 2>&1 || true
  for _ in 1 2 3 4 5; do
    pgrep -x cu_fuse >/dev/null 2>&1 || break
    sleep 1
  done

  if pgrep -x cu_fuse >/dev/null 2>&1; then
    pkill -KILL -x cu_fuse >/dev/null 2>&1 || true
  fi
fi

graceful_unmount

if [ -d "${mnt}" ] && ! mount_is_active; then
  rmdir "${mnt}" >/dev/null 2>&1 || true
fi

exit 0
EOF

chmod 700 "${cleanup_helper}"
node_count=$(wc -l < "${SLURM_NODEFILE}")

echo "Stopping Copper Gracefully On All Nodes : Start"
srun --overlap --kill-on-bad-exit=0 -N "${node_count}" -n "${node_count}" --ntasks-per-node=1 --cpu-bind=none \
  bash "${cleanup_helper}" "${CU_FUSE_MNT_VIEWDIR}" || true
echo "Stopping Copper Gracefully On All Nodes : End"
