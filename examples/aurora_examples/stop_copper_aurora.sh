#!/bin/bash -x
CU_FUSE_MNT_VIEWDIR=/tmp/${USER}/copper
log_dir=~/copper-logs

while getopts "d:v:" opt; do
  case ${opt} in
    d ) log_dir=$OPTARG ;;
    v ) CU_FUSE_MNT_VIEWDIR=$OPTARG ;;
    \? ) echo "Usage: cmd [-d log_dir] [-v CU_FUSE_MNT_VIEWDIR]" ; exit 1 ;;
  esac
done

job_id=${PBS_JOBID%%.*}
log_dir=${log_dir%/}/${job_id}
node_file_copy=${log_dir}/logs/node_file.txt

if [ -f "${node_file_copy}" ]; then
  hostfile="${node_file_copy}"
else
  hostfile="${PBS_NODEFILE}"
fi

cleanup_helper=${log_dir}/logs/stop_copper_remote.sh
mkdir -p "$(dirname "${cleanup_helper}")"

cat > "${cleanup_helper}" <<'EOF'
#!/bin/bash
set +e
ulimit -c 0

mnt="$1"
cmd_timeout_s=10

run_with_timeout() {
  if command -v timeout >/dev/null 2>&1; then
    timeout "${cmd_timeout_s}s" "$@"
  else
    "$@"
  fi
}

mount_is_active() {
  if command -v findmnt >/dev/null 2>&1; then
    run_with_timeout findmnt -T "${mnt}" >/dev/null 2>&1
  elif command -v mountpoint >/dev/null 2>&1; then
    run_with_timeout mountpoint -q "${mnt}" >/dev/null 2>&1
  else
    run_with_timeout grep -qs " ${mnt} " /proc/mounts
  fi
}

graceful_unmount() {
  local tries=0
  while mount_is_active && [ "${tries}" -lt 5 ]; do
    run_with_timeout fusermount3 -u "${mnt}" >/dev/null 2>&1 || true
    mount_is_active || break
    sleep 2
    tries=$((tries + 1))
  done

  if mount_is_active; then
    run_with_timeout fusermount3 -uz "${mnt}" >/dev/null 2>&1 || true
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
  run_with_timeout rmdir "${mnt}" >/dev/null 2>&1 || true
fi

exit 0
EOF

chmod 700 "${cleanup_helper}"

echo "Stopping Copper Gracefully On All Nodes : Start"
clush --hostfile "${hostfile}" "bash '${cleanup_helper}' '${CU_FUSE_MNT_VIEWDIR}' || true" || true
echo "Stopping Copper Gracefully On All Nodes : End"
