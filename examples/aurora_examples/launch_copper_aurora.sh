#!/bin/bash -x

echo "Launching Copper Gracefully On All Nodes : Start"
echo "${COPPER_ROOT}"
CUPATH=${COPPER_ROOT}/build/cu_fuse
DISCOVERY_HELPER=${COPPER_ROOT}/build/list_cxi_hsn_thallium
CU_FUSE_MNT_VIEWDIR=/tmp/${USER}/copper
facility_address_book=${COPPER_ROOT}/build/alcf_aurora_copper_addressbook.txt
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
physcpubind="48,49,50,51" # Aurora service cores for copper 
net_type="cxi"            # Aurora endpoint cxi points to "cxi://cxi1"

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

job_id=${PBS_JOBID%%.*}
log_dir=${log_dir%/}/${job_id}
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

# The Aurora example keeps the launcher self-contained and uses clush against
# the PBS node file instead of the shared Slurm-oriented wrapper logic.
options="-tpath / -vpath ${CU_FUSE_MNT_VIEWDIR} -log_level ${log_level} -log_type ${log_type} -log_output_dir ${log_dir} -net_type ${net_type} -trees ${trees} -nf ${node_file_copy} -max_cacheable_byte_size ${max_cacheable_byte_size} -md_enoent_ttl_ms ${md_enoent_ttl_ms}${profile_options} -facility_address_book ${prepared_address_book} -prefiltered_address_book 1 -s ${CU_FUSE_MNT_VIEWDIR}"

read -r -d '' CMD <<EOM
numactl --physcpubind=${physcpubind} ${CUPATH} ${options}
EOM

clush --hostfile "${node_file_copy}" "${CMD}"

echo "sleeping for ${sleeptime} s"
sleep "${sleeptime}"s

echo "Launching Copper Gracefully On All Nodes : End"
