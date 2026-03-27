#!/bin/bash -x

echo "Launching Copper Gracefully On All Nodes : Start"
echo "${COPPER_ROOT}"
CUPATH=${COPPER_ROOT}/build/cu_fuse
DISCOVERY_HELPER=${COPPER_ROOT}/build/list_cxi_hsn_thallium
CU_FUSE_MNT_VIEWDIR=/tmp/${USER}/copper
facility_address_book=${COPPER_ROOT}/build/olcf_frontier_copper_addressbook.txt
address_book_source="facility"
log_level=0
log_dir=~/copper-logs
log_type="file"
trees=1
max_cacheable_byte_size=$((10 * 1024 * 1024))
md_enoent_ttl_ms=2000
profile_metrics=0
profile_top_n=0
profile_paths_full=0
profile_snapshot_interval_s=0
sleeptime=10
physcpubind="1,2"           # Frontier service cores for copper or "1,2,65,66"
net_type="cxi://cxi1"       # Frontier endpoint cxi

while getopts "l:t:d:T:M:E:PN:AI:s:b:F:a:n:v:" opt; do
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
    \? ) echo "Usage: cmd [-l log_level 0-5] [-t log_type] [-d log_dir_base] [-T trees] [-M max_cacheable_byte_size] [-E md_enoent_ttl_ms] [-P enable_profile_metrics] [-N profile_top_n] [-A profile_paths_full] [-I profile_snapshot_interval_s] [-s sleeptime] [-b physcpubind] [-F facility_address_book] [-a address_book_source(facility|discover)] [-n net_type] [-v CU_FUSE_MNT_VIEWDIR]" ;;
  esac
done

log_dir=${log_dir%/}/${SLURM_JOB_ID}
logs_dir=${log_dir}/logs
tables_dir=${log_dir}/tables
profiling_dir=${log_dir}/profiling
profiling_final_dir=${profiling_dir}/final
profiling_cluster_dir=${profiling_dir}/cluster
node_file_copy=${logs_dir}/node_file.txt

prepared_address_book=${logs_dir}/copper_address_book.txt
prepared_address_book_full_output=${logs_dir}/copper_address_book_full_output.txt
prepare_address_book_awk=${logs_dir}/prepare_address_book.awk
select_discovered_address_book_awk=${logs_dir}/select_discovered_address_book.awk

prepare_address_book_from_facility() {
  cp "${facility_address_book}" "${prepared_address_book_full_output}" || return 1

  cat > "${prepare_address_book_awk}" <<'EOF'
NR==FNR {
  order[++n]=$1
  need[$1]=1
  next
}
($1 in need) {
  addr[$1]=$2
}
END {
  for(i=1;i<=n;i++) {
    if(order[i] in addr) {
      print order[i] " " addr[order[i]]
    }
  }
}
EOF

  srun --overlap -N 1 -n 1 --ntasks-per-node=1 \
    env NODEFILE_PATH="${node_file_copy}" FACILITY_BOOK="${facility_address_book}" OUTPUT_PATH="${prepared_address_book}" AWK_SCRIPT="${prepare_address_book_awk}" \
    bash -lc 'awk -f "$AWK_SCRIPT" "$NODEFILE_PATH" "$FACILITY_BOOK" > "$OUTPUT_PATH"'
}

prepare_address_book_from_discovery() {
  local selected_column
  if [[ "${net_type}" =~ ^cxi://cxi([0-9]+)$ ]]; then
    selected_column="cxi${BASH_REMATCH[1]}"
  else
    echo "discover mode currently supports net_type values like cxi://cxi1; got ${net_type}"
    return 1
  fi

  if [ ! -x "${DISCOVERY_HELPER}" ]; then
    echo "discover mode requested but helper is missing or not executable: ${DISCOVERY_HELPER}"
    return 1
  fi

  srun --overlap -N "${SLURM_NNODES}" --ntasks-per-node=1 "${DISCOVERY_HELPER}" > "${prepared_address_book_full_output}"

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
echo "SLURM_NODEFILE             : ${SLURM_NODEFILE}"
echo "physcpubind                : ${physcpubind}"
echo "facility_address_book      : ${facility_address_book}"
echo "address_book_source        : ${address_book_source}"
echo "prepared_address_book      : ${prepared_address_book}"
echo "prepared_address_book_full : ${prepared_address_book_full_output}"
echo "discovery_helper           : ${DISCOVERY_HELPER}"
echo "net_type                   : ${net_type}"

mkdir -p "${logs_dir}" "${tables_dir}/final" "${profiling_final_dir}" "${profiling_cluster_dir}"
scontrol show hostnames "$SLURM_JOB_NODELIST" > "${node_file_copy}"
clush --hostfile "${node_file_copy}" "fusermount3 -u ${CU_FUSE_MNT_VIEWDIR}" >/dev/null 2>&1
clush --hostfile "${node_file_copy}" "rm -rf ${CU_FUSE_MNT_VIEWDIR}" >/dev/null 2>&1
clush --hostfile "${node_file_copy}" "mkdir -p ${CU_FUSE_MNT_VIEWDIR}" >/dev/null 2>&1

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

profile_options=""
if [ "${profile_metrics}" -eq 1 ]; then
  profile_options="${profile_options} -profile_metrics"
fi
if [ "${profile_top_n}" -gt 0 ]; then
  profile_options="${profile_options} -profile_top_n ${profile_top_n}"
fi
if [ "${profile_paths_full}" -eq 1 ]; then
  profile_options="${profile_options} -profile_paths_full"
fi
if [ "${profile_snapshot_interval_s}" -gt 0 ]; then
  profile_options="${profile_options} -profile_snapshot_interval_s ${profile_snapshot_interval_s}"
fi

options=" -f -tpath / -vpath ${CU_FUSE_MNT_VIEWDIR} -log_level ${log_level} -log_type ${log_type} -log_output_dir ${log_dir} -net_type ${net_type} -trees ${trees} -nf ${node_file_copy} -max_cacheable_byte_size ${max_cacheable_byte_size} -md_enoent_ttl_ms ${md_enoent_ttl_ms}${profile_options} -facility_address_book ${prepared_address_book} -prefiltered_address_book 1 -s ${CU_FUSE_MNT_VIEWDIR}"

srun --overlap -N ${SLURM_NNODES} --ntasks-per-node=1 --cpus-per-task=2 --cpu-bind=verbose,none \
  bash -c '
    echo "host=$(hostname)"
    taskset -c '"${physcpubind}"' bash -c '"'"'
      echo "allowed=$(grep Cpus_allowed_list /proc/self/status | awk "{print \$2}")"
      echo "current_cpu=$(ps -o psr= -p $$)"
      exec '"${CUPATH}"' '"${options}"'
    '"'"'
  ' &

echo "sleeping for ${sleeptime} s"
sleep "${sleeptime}"s

echo "Launching Copper Gracefully On All Nodes : End"
