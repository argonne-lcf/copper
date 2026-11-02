#!/bin/bash

set -o pipefail

shell_quote() {
  printf '%q' "$1"
}

append_remote_arg() {
  local quoted
  printf -v quoted '%q' "$2"
  printf -v "$1" '%s %s' "${!1}" "${quoted}"
}

serialize_runtime_environment() {
  local name value quoted serialized=""
  while IFS= read -r name; do
    case "${name}" in
      PATH|LD_LIBRARY_PATH|COPPER_ROOT|FI_*|MARGO_*|MERCURY_*|THALLIUM_*|ABT_*|CXI_*|PSM2_*|LIBFABRIC_*)
        value=${!name}
        printf -v quoted '%q' "${value}"
        serialized+=" ${name}=${quoted}"
        ;;
    esac
  done < <(compgen -e)
  printf '%s' "${serialized}"
}

echo "Launching Copper Gracefully On All Nodes : Start"
echo "${COPPER_ROOT}"
CUPATH=${COPPER_ROOT}/build/cu_fuse
DISCOVERY_HELPER=${COPPER_ROOT}/build/list_cxi_hsn_thallium
SCALE_CHECK=${COPPER_ROOT}/scripts/copper-scale-check
[ -x "${SCALE_CHECK}" ] || SCALE_CHECK=$(cd "$(dirname "$0")" && pwd)/copper-scale-check
CU_FUSE_MNT_VIEWDIR=/tmp/${USER}/copper
facility_address_book=${COPPER_ROOT}/build/alcf_aurora_copper_addressbook.txt
address_book_source="facility"
log_level=0
log_dir=~/copper-logs
log_type="file"
trees=1
readiness_timeout=120
readiness_backoff=2
probe_path=""
recovery_hook=""
recovery_attempts_max=0
startup_log_level=""
operation_log_level=""
engine_startup_timeout_ms=120000
parent_ready_timeout_ms=120000
max_cacheable_byte_size=$((10 * 1024 * 1024))
md_enoent_ttl_ms=2000
profile_metrics=0
profile_top_n=0
profile_paths_full=0
profile_snapshot_interval_s=0
sleeptime=10
physcpubind="48,49,50,51" # Aurora service cores for copper 
net_type="cxi"            # Aurora endpoint cxi points to "cxi://cxi1"

usage() {
  echo "Usage: $0 [-T trees|auto] [-s minimum_delay] [-R readiness_timeout] [-B readiness_backoff] [-p exact_probe_path] [-H recovery_hook -K attempts] [-L startup_log_level] [-O operation_log_level] [-X engine_timeout_ms] [-Y parent_timeout_ms] [-l 0-5] [-d log_dir] [-v mount]"
}

while getopts "hl:t:d:T:M:E:PN:AI:s:b:F:a:n:v:R:B:p:H:K:L:O:X:Y:" opt; do
  case ${opt} in
    l ) log_level=$OPTARG ;;
    t ) log_type=$OPTARG ;;
    d ) log_dir=$OPTARG ;;
    T ) trees=$OPTARG ;;
    M ) max_cacheable_byte_size=$OPTARG ;;
    E ) md_enoent_ttl_ms=$OPTARG ;;
    P ) profile_metrics=1 ;;
    N ) profile_metrics=1; profile_top_n=$OPTARG ;;
    A ) profile_metrics=1; profile_paths_full=1 ;;
    I ) profile_metrics=1; profile_snapshot_interval_s=$OPTARG ;;
    s ) sleeptime=$OPTARG ;;
    b ) physcpubind=$OPTARG ;;
    F ) facility_address_book=$OPTARG ;;
    a ) address_book_source=$OPTARG ;;
    n ) net_type=$OPTARG ;;
    v ) CU_FUSE_MNT_VIEWDIR=$OPTARG ;;
    R ) readiness_timeout=$OPTARG ;;
    B ) readiness_backoff=$OPTARG ;;
    p ) probe_path=$OPTARG ;;
    H ) recovery_hook=$OPTARG ;;
    K ) recovery_attempts_max=$OPTARG ;;
    L ) startup_log_level=$OPTARG ;;
    O ) operation_log_level=$OPTARG ;;
    X ) engine_startup_timeout_ms=$OPTARG ;;
    Y ) parent_ready_timeout_ms=$OPTARG ;;
    h ) usage; exit 0 ;;
    \? ) usage; exit 2 ;;
  esac
done

job_id=${PBS_JOBID%%.*}
log_dir=${log_dir%/}/${job_id}
logs_dir=${log_dir}/logs
tables_dir=${log_dir}/tables
profiling_dir=${log_dir}/profiling
profiling_final_dir=${profiling_dir}/final
profiling_cluster_dir=${profiling_dir}/cluster
node_file_copy=${logs_dir}/node_file.txt
startup_summary=${log_dir}/startup-summary.json
scale_manifest=${logs_dir}/scale-readiness.json

prepared_address_book=${logs_dir}/copper_address_book.txt
prepared_address_book_full_output=${logs_dir}/copper_address_book_full_output.txt
prepare_address_book_awk=${logs_dir}/prepare_address_book.awk
select_discovered_address_book_awk=${logs_dir}/select_discovered_address_book.awk

prepare_address_book_from_facility() {
  cp "${facility_address_book}" "${prepared_address_book_full_output}" || return 1

  cat > "${prepare_address_book_awk}" <<'EOF'
function short_host(hostname) {
  sub(/\..*$/, "", hostname)
  return hostname
}

NR==FNR {
  original_order[++n]=$1
  short=short_host($1)
  normalized_order[n]=short
  need[short]=1
  if(!(short in display_name)) {
    display_name[short]=short
  }
  next
}
{
  short=short_host($1)
}
(short in need) {
  addr[short]=$2
  display_name[short]=short
}
END {
  for(i=1;i<=n;i++) {
    short=normalized_order[i]
    if(short in addr) {
      print display_name[short] " " addr[short]
    }
  }
}
EOF

  awk -f "${prepare_address_book_awk}" "${PBS_NODEFILE}" "${facility_address_book}" > "${prepared_address_book}"
}

prepare_address_book_from_discovery() {
  local selected_column
  case "${net_type}" in
    cxi|cxi0|cxi://cxi0)
      selected_column="cxi0"
      ;;
    cxi[1-7]|cxi://cxi[1-7])
      selected_column="${net_type#cxi://}"
      ;;
    *)
      echo "unsupported Aurora net_type for discovery: ${net_type}; expected cxi, cxi0, or cxi1-cxi7"
      return 1
      ;;
  esac

  if [ ! -x "${DISCOVERY_HELPER}" ]; then
    echo "discover mode requested but helper is missing or not executable: ${DISCOVERY_HELPER}"
    return 1
  fi

  local nnodes
  nnodes=$(wc -l < "${PBS_NODEFILE}")
  mpirun --np "${nnodes}" --ppn 1 "${DISCOVERY_HELPER}" > "${prepared_address_book_full_output}" || return $?

  cat > "${select_discovered_address_book_awk}" <<'EOF'
BEGIN {
  FS=" \\| "
}
NR==1 {
  for(i=1;i<=NF;i++) {
    header[$i]=i
  }
  if(!(target_col in header)) {
    print "missing target column: " target_col > "/dev/stderr"
    exit 2
  }
  col_idx=header[target_col]
  next
}
{
  if($1 != "" && col_idx > 0 && $col_idx != "" && $col_idx != "NA") {
    print $1 " " $col_idx
  }
}
EOF

  awk -v target_col="${selected_column}" -f "${select_discovered_address_book_awk}" \
    "${prepared_address_book_full_output}" > "${prepared_address_book}"
}

echo "log_level                  : ${log_level}"
echo "log_type                   : ${log_type}"
echo "trees                      : ${trees}"
echo "max_cacheable_byte_size    : ${max_cacheable_byte_size}"
echo "md_enoent_ttl_ms           : ${md_enoent_ttl_ms}"
echo "profile_metrics            : ${profile_metrics}"
echo "profile_top_n              : ${profile_top_n}"
echo "profile_paths_full         : ${profile_paths_full}"
echo "profile_snapshot_interval_s: ${profile_snapshot_interval_s}"
echo "sleeptime                  : ${sleeptime}"
echo "CU_FUSE_MNT_VIEWDIR        : ${CU_FUSE_MNT_VIEWDIR}"
echo "LOGDIR                     : ${log_dir}"
echo "logs_dir                   : ${logs_dir}"
echo "tables_dir                 : ${tables_dir}"
echo "profiling_final_dir        : ${profiling_final_dir}"
echo "profiling_cluster_dir      : ${profiling_cluster_dir}"
echo "PBS_NODEFILE               : ${PBS_NODEFILE}"
echo "physcpubind                : ${physcpubind}"
echo "facility_address_book      : ${facility_address_book}"
echo "address_book_source        : ${address_book_source}"
echo "prepared_address_book      : ${prepared_address_book}"
echo "prepared_address_book_full : ${prepared_address_book_full_output}"
echo "discovery_helper           : ${DISCOVERY_HELPER}"
echo "net_type                   : ${net_type}"

mkdir -p "${logs_dir}" "${tables_dir}/final" "${profiling_final_dir}" "${profiling_cluster_dir}"
cp "${PBS_NODEFILE}" "${node_file_copy}"
expected_nodes=$(awk 'NF {h=$1; sub(/\..*$/, "", h); if (!seen[h]++) n++} END {print n+0}' "${node_file_copy}")
if [ "${trees}" = auto ]; then trees=$(( (expected_nodes + 127) / 128 )); fi
if ! [[ "${trees}" =~ ^[1-9][0-9]*$ ]]; then echo "invalid trees value: ${trees}"; exit 2; fi
if [ ! -x "${SCALE_CHECK}" ]; then echo "missing executable readiness tool: ${SCALE_CHECK}"; exit 2; fi
if [ ! -x "${COPPER_ROOT}/build/cu_fuse_probe" ]; then echo "missing executable RPC readiness probe: ${COPPER_ROOT}/build/cu_fuse_probe"; exit 2; fi
if ! [[ "${recovery_attempts_max}" =~ ^[0-9]+$ ]]; then echo "invalid recovery attempts: ${recovery_attempts_max}"; exit 2; fi
segment_size=$(( (expected_nodes + trees - 1) / trees ))
tree_depth=0
tree_capacity=1
while [ "${tree_capacity}" -lt "${segment_size}" ]; do tree_capacity=$((tree_capacity * 2)); tree_depth=$((tree_depth + 1)); done
echo "preflight: nodes=${expected_nodes} trees=${trees} segment_size=${segment_size} depth=${tree_depth} timeout=${readiness_timeout}s backoff=${readiness_backoff}s minimum_delay=${sleeptime}s"
if [ "${trees}" -gt "${expected_nodes}" ]; then echo "warning: trees (${trees}) exceeds allocation nodes (${expected_nodes})"; fi
if [ "${segment_size}" -gt 128 ]; then echo "warning: overlay segment size ${segment_size} exceeds recommended maximum 128; use more trees"; fi
quoted_mount=$(shell_quote "${CU_FUSE_MNT_VIEWDIR}")
runtime_environment=$(serialize_runtime_environment)
remote_env="env${runtime_environment}"
# An absent pre-existing mount is normal, but directory reset failures are not.
clush --hostfile "${node_file_copy}" "${remote_env} fusermount3 -u ${quoted_mount}" >/dev/null 2>&1 || true
if ! clush --hostfile "${node_file_copy}" "${remote_env} rm -rf ${quoted_mount} && ${remote_env} mkdir -p ${quoted_mount}" >/dev/null 2>&1; then
  echo "remote mount directory preparation failed: ${CU_FUSE_MNT_VIEWDIR}"
  exit 1
fi

rm -f "${prepared_address_book}" "${prepared_address_book_full_output}" "${prepare_address_book_awk}" "${select_discovered_address_book_awk}"

echo "Preparing job-local address book once before full cu_fuse launch"
prepare_address_book_start_epoch=$(date +%s)
prepare_address_book_rc=0
case "${address_book_source}" in
  facility)
    prepare_address_book_from_facility || prepare_address_book_rc=$?
    ;;
  discover)
    prepare_address_book_from_discovery || prepare_address_book_rc=$?
    ;;
  *)
    echo "invalid address_book_source: ${address_book_source}; expected facility or discover"
    prepare_address_book_rc=1
    ;;
esac
prepare_address_book_end_epoch=$(date +%s)
echo "prepared address book generation took $((prepare_address_book_end_epoch - prepare_address_book_start_epoch)) s"

if [ ${prepare_address_book_rc} -ne 0 ] || [ ! -s "${prepared_address_book}" ]; then
  echo "prepared address book generation failed (rc=${prepare_address_book_rc}): ${prepared_address_book}"
  exit 1
fi
if ! awk 'NF != 2 || $1 !~ /^[[:alnum:]_.-]+$/ || $2 !~ /^[[:alnum:]+._:/%-]+$/ {exit 1} END {if (NR == 0) exit 1}' "${prepared_address_book}" ||
   [ "$(awk 'NF {h=$1; sub(/\..*$/, "", h); if (!seen[h]++) n++} END {print n+0}' "${prepared_address_book}")" -ne "${expected_nodes}" ]; then
  echo "address book validation failed: expected exactly ${expected_nodes} well-formed host/address rows"
  exit 1
fi

profile_options=()
if [ "${profile_metrics}" -eq 1 ]; then
  profile_options+=("-profile_metrics")
fi
if [ "${profile_top_n}" -gt 0 ]; then
  profile_options+=("-profile_top_n" "${profile_top_n}")
fi
if [ "${profile_paths_full}" -eq 1 ]; then
  profile_options+=("-profile_paths_full")
fi
if [ "${profile_snapshot_interval_s}" -gt 0 ]; then
  profile_options+=("-profile_snapshot_interval_s" "${profile_snapshot_interval_s}")
fi

options=(
  -tpath / -vpath "${CU_FUSE_MNT_VIEWDIR}"
  -log_level "${log_level}" -log_type "${log_type}" -log_output_dir "${log_dir}"
  -net_type "${net_type}" -trees "${trees}" -nf "${node_file_copy}"
  -max_cacheable_byte_size "${max_cacheable_byte_size}"
  -md_enoent_ttl_ms "${md_enoent_ttl_ms}"
  "${profile_options[@]}"
  -facility_address_book "${prepared_address_book}" -prefiltered_address_book 1
  -s "${CU_FUSE_MNT_VIEWDIR}"
)
[ -n "${startup_log_level}" ] && options+=("-startup_log_level" "${startup_log_level}")
[ -n "${operation_log_level}" ] && options+=("-operation_log_level" "${operation_log_level}")
options+=("-engine_startup_timeout_ms" "${engine_startup_timeout_ms}")
options+=("-parent_ready_timeout_ms" "${parent_ready_timeout_ms}")

CMD="ulimit -c unlimited && ${remote_env}"
append_remote_arg CMD "numactl"
append_remote_arg CMD "--physcpubind=${physcpubind}"
append_remote_arg CMD "${CUPATH}"
for option in "${options[@]}"; do
  append_remote_arg CMD "${option}"
done

launch_epoch=$(date +%s)
if ! clush --hostfile "${node_file_copy}" "${CMD}"; then
  echo "Copper rank startup failed"
  exit 1
fi

echo "sleeping for ${sleeptime} s (minimum backward-compatible delay)"
sleep "${sleeptime}"s

scale_args=(--hostfile "${node_file_copy}" --mount "${CU_FUSE_MNT_VIEWDIR}" --rpc-probe-helper "${COPPER_ROOT}/build/cu_fuse_probe" --network-type "${net_type}" --provider-id 0 --address-book "${prepared_address_book}" --timeout "${readiness_timeout}" --backoff "${readiness_backoff}" --manifest "${scale_manifest}" --registration-dir "${logs_dir}" --registration-since-epoch "${launch_epoch}")
[ -n "${probe_path}" ] && scale_args+=(--probe-path "${probe_path}")
readiness_rc=0
"${SCALE_CHECK}" "${scale_args[@]}" || readiness_rc=$?
recovery_attempts=0
while [ "${readiness_rc}" -ne 0 ] && [ "${recovery_attempts}" -lt "${recovery_attempts_max}" ]; do
  if [ -z "${recovery_hook}" ] || [ ! -x "${recovery_hook}" ]; then
    echo "recovery enabled but hook is absent or not executable"
    break
  fi
  failed_hostfile=${logs_dir}/recovery-failed-hosts-$((recovery_attempts + 1)).txt
  SCALE_MANIFEST="${scale_manifest}" FAILED_HOSTFILE="${failed_hostfile}" python3 - <<'PY'
import json, os
d=json.load(open(os.environ["SCALE_MANIFEST"]))
failed=sorted(h for h,v in d.get("nodes",{}).items() if not v.get("ready"))
with open(os.environ["FAILED_HOSTFILE"],"w") as f:
    for host in failed: f.write(host+"\n")
PY
  [ -s "${failed_hostfile}" ] || break
  recovery_attempts=$((recovery_attempts + 1))
  echo "running bounded recovery attempt ${recovery_attempts}/${recovery_attempts_max} for $(wc -l < "${failed_hostfile}") failed host(s)"
  "${recovery_hook}" "${failed_hostfile}" "${recovery_attempts}" || break
  readiness_rc=0
  "${SCALE_CHECK}" "${scale_args[@]}" || readiness_rc=$?
done
if [ "${readiness_rc}" -ne 0 ]; then
  [ -n "${recovery_hook}" ] && [ "${recovery_attempts_max}" -eq 0 ] && echo "recovery disabled (set -K to a positive bounded attempt count)"
  echo "Copper readiness barrier failed (mount verification failed or provider/path readiness incomplete)"
fi
address_sha256=$(sha256sum "${prepared_address_book}" | cut -d' ' -f1)
binary_sha256=$(sha256sum "${CUPATH}" 2>/dev/null | cut -d' ' -f1 || true)
source_revision=$(git -C "${COPPER_ROOT}" rev-parse HEAD 2>/dev/null || true)
export STARTUP_SUMMARY="${startup_summary}" SCALE_MANIFEST="${scale_manifest}" ADDRESS_SHA256="${address_sha256}" ADDRESS_MODE="${address_book_source}" SOURCE_REVISION="${source_revision}" COPPER_BINARY="${CUPATH}" BINARY_SHA256="${binary_sha256}" TREES="${trees}" EXPECTED_NODES="${expected_nodes}" PROBE_PATH="${probe_path}" READINESS_RC="${readiness_rc}" RECOVERY_ATTEMPTS="${recovery_attempts}"
python3 - <<'PY'
import json, os, pathlib, tempfile
p = pathlib.Path(os.environ["STARTUP_SUMMARY"])
try: readiness = json.loads(pathlib.Path(os.environ["SCALE_MANIFEST"]).read_text())
except Exception as exc: readiness = {"status": "failed", "error": str(exc), "nodes": {}}
failed = sorted(h for h, v in readiness.get("nodes", {}).items() if not v.get("ready"))
counts = dict(readiness.get("counts") or {})
counts["launched"] = int(os.environ["EXPECTED_NODES"])
payload = {"status": "ready" if os.environ["READINESS_RC"] == "0" else "failed",
 "trees": int(os.environ["TREES"]), "expected_nodes": int(os.environ["EXPECTED_NODES"]),
 "probe_path": os.environ["PROBE_PATH"] or None,
 "address_book": {"mode": os.environ["ADDRESS_MODE"], "sha256": os.environ["ADDRESS_SHA256"]},
 "provenance": {"source_revision": os.environ["SOURCE_REVISION"] or None, "binary": os.environ["COPPER_BINARY"], "binary_sha256": os.environ["BINARY_SHA256"] or None},
 "phase_failure_hosts": {"readiness": failed}, "counts": counts,
 "recovery_attempts": int(os.environ["RECOVERY_ATTEMPTS"]),
 "registration_quantiles_us": readiness.get("registration_quantiles_us"),
 "readiness": readiness}
fd, tmp = tempfile.mkstemp(prefix=p.name+".", dir=p.parent)
with os.fdopen(fd, "w") as f: json.dump(payload, f, indent=2, sort_keys=True); f.write("\n"); f.flush(); os.fsync(f.fileno())
os.replace(tmp, p)
PY
[ "${readiness_rc}" -eq 0 ] || exit "${readiness_rc}"

echo "Launching Copper Gracefully On All Nodes : End"
