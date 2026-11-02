#include "profiler_core.h"
#include "operation_types.h"
#include <algorithm>
#include <chrono>
#include <bit>
#include <stdexcept>
#include <cctype>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <numeric>
#include <sstream>
#include <system_error>
#include <unistd.h>

namespace {
uint64_t quantile(const std::vector<uint64_t>& v,double p){if(v.empty())return 0; auto i=static_cast<size_t>((v.size()-1)*p+0.999999);return v[std::min(i,v.size()-1)];}
const char* source_name(ProfileSource s){switch(s){case ProfileSource::metadata:return "metadata";case ProfileSource::data:return "data";case ProfileSource::tree:return "tree";case ProfileSource::bypass:return "bypass";default:return "none";}}
std::string csv_escape(const std::string& v){std::string r="\"";for(char c:v)r+=c=='"'?"\"\"":std::string(1,c);return r+'"';}
std::string json_escape(const std::string& v){std::ostringstream o;for(unsigned char c:v){switch(c){case '"':o<<"\\\"";break;case '\\':o<<"\\\\";break;case '\b':o<<"\\b";break;case '\f':o<<"\\f";break;case '\n':o<<"\\n";break;case '\r':o<<"\\r";break;case '\t':o<<"\\t";break;default:if(c<0x20)o<<"\\u"<<std::hex<<std::setw(4)<<std::setfill('0')<<unsigned(c)<<std::dec;else o<<char(c);}}return o.str();}
std::string filename_component(const std::string& value){std::string out;out.reserve(value.size());for(unsigned char c:value)out+=std::isalnum(c)||c=='-'||c=='_'||c=='.'?char(c):'_';while(out.find("..")!=std::string::npos)out.replace(out.find(".."),2,"__");return out.empty()?"unknown":out;}
bool atomic_write(const std::filesystem::path&p,const std::string&d,std::string&e){auto t=p.string()+".tmp."+std::to_string(getpid());std::ofstream o(t,std::ios::binary|std::ios::trunc);if(!o){e="open "+t;return false;}o<<d;o.flush();if(!o){e="write "+t;o.close();std::filesystem::remove(t);return false;}o.close();std::error_code ec;std::filesystem::rename(t,p,ec);if(ec){e="rename "+t+": "+ec.message();std::filesystem::remove(t);return false;}return true;}
std::string errors_string(const std::map<int,uint64_t>&e){std::ostringstream o;bool f=true;for(auto [n,c]:e){if(!f)o<<';';o<<n<<':'<<c;f=false;}return o.str();}
}
// Dependency-free SHA-256 (FIPS 180-4), hashes binary bytes including NUL.
std::string profiler_sha256(const std::string& bytes) {
    static constexpr uint32_t k[64]={
        0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
        0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
        0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
        0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
        0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
        0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
        0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
        0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2};
    std::array<uint32_t,8> h={0x6a09e667,0xbb67ae85,0x3c6ef372,0xa54ff53a,0x510e527f,0x9b05688c,0x1f83d9ab,0x5be0cd19};
    const uint64_t bits=uint64_t(bytes.size())*8;
    std::string padded=bytes; padded.push_back(char(0x80));
    while(padded.size()%64!=56)padded.push_back(0);
    for(int i=7;i>=0;--i)padded.push_back(char(bits>>(8*i)));
    for(size_t offset=0;offset<padded.size();offset+=64) {
        uint32_t w[64]{};
        for(size_t i=0;i<16;++i)for(size_t j=0;j<4;++j)w[i]=(w[i]<<8)|static_cast<unsigned char>(padded[offset+i*4+j]);
        for(size_t i=16;i<64;++i) {
            const auto s0=std::rotr(w[i-15],7)^std::rotr(w[i-15],18)^(w[i-15]>>3);
            const auto s1=std::rotr(w[i-2],17)^std::rotr(w[i-2],19)^(w[i-2]>>10);
            w[i]=w[i-16]+s0+w[i-7]+s1;
        }
        auto a=h[0],b=h[1],c=h[2],d=h[3],e=h[4],f=h[5],g=h[6],v=h[7];
        for(size_t i=0;i<64;++i){
            auto t1=v+(std::rotr(e,6)^std::rotr(e,11)^std::rotr(e,25))+((e&f)^(~e&g))+k[i]+w[i];
            auto t2=(std::rotr(a,2)^std::rotr(a,13)^std::rotr(a,22))+((a&b)^(a&c)^(b&c));
            v=g;g=f;f=e;e=d+t1;d=c;c=b;b=a;a=t1+t2;
        }
        h[0]+=a;h[1]+=b;h[2]+=c;h[3]+=d;h[4]+=e;h[5]+=f;h[6]+=g;h[7]+=v;
    }
    std::ostringstream out;out<<std::hex<<std::setfill('0');for(auto value:h)out<<std::setw(8)<<value;return out.str();
}
uint64_t LatencyHistogram::lower_bound(size_t b) {
    if(b >= bucket_count) throw std::out_of_range("histogram bucket");
    return b ? uint64_t{1} << (b-1) : 0;
}
uint64_t LatencyHistogram::upper_bound(size_t b) {
    if(b >= bucket_count) throw std::out_of_range("histogram bucket");
    return b == 64 ? UINT64_MAX : (b ? (uint64_t{1} << b)-1 : 0);
}
void LatencyHistogram::observe(uint64_t ns) { ++counts[std::bit_width(ns)]; }
void LatencyHistogram::merge(const LatencyHistogram& other) {
    for(size_t i=0;i<bucket_count;++i) if(UINT64_MAX-counts[i]<other.counts[i]) throw std::overflow_error("histogram merge");
    for(size_t i=0;i<bucket_count;++i) counts[i]+=other.counts[i];
}
uint64_t LatencyHistogram::sample_count() const {
    uint64_t n=0; for(auto count:counts) { if(UINT64_MAX-n<count) throw std::overflow_error("histogram count"); n+=count; } return n;
}
uint64_t LatencyHistogram::quantile_upper_bound(uint64_t numerator,uint64_t denominator) const {
    if(!denominator || numerator>denominator) throw std::invalid_argument("quantile fraction");
    const auto n=sample_count(); if(!n)return 0;
    // Portable ceil(n * numerator / denominator), without overflowing multiplication.
    uint64_t rank=0, remainder=0;
    for(int bit=63;bit>=0;--bit) {
        const bool carry=remainder>=denominator-remainder;
        remainder=carry?remainder-(denominator-remainder):remainder+remainder;
        rank=rank*2+carry;
        if((n>>bit)&1) {
            const bool add_carry=remainder>=denominator-numerator;
            remainder=add_carry?remainder-(denominator-numerator):remainder+numerator;
            rank+=add_carry;
        }
    }
    if(remainder)++rank;
    if(!rank)rank=1;
    uint64_t cumulative=0;for(size_t i=0;i<bucket_count;++i){cumulative+=counts[i];if(cumulative>=rank)return upper_bound(i);}return UINT64_MAX;
}
ProfilerCore::ProfilerCore(ProfilerConfig c,size_t n):config_(c),operations_(n),samples_(n){
    if(c.top_n>1000000 || c.latency_capacity>65536)throw std::invalid_argument("profiler capacity exceeds supported maximum");
    const char* mode=std::getenv("COPPER_PROFILE_PATH_MODE");
    if(mode && *mode) {
        const std::string value(mode);
        if(value=="heavy-hitter")config_.heavy_hitters=true;
        else if(value=="admitted-set")config_.heavy_hitters=false;
        else throw std::invalid_argument("COPPER_PROFILE_PATH_MODE must be heavy-hitter or admitted-set");
    }
    if(config_.heavy_hitters && (!config_.top_n || config_.full_paths))
        throw std::invalid_argument("heavy-hitter requires positive top_n and full_paths=false");
}
void ProfilerCore::record_path_locked(const std::string&p,ProfileSource src,ProfileOutcome*out,int err,MetadataTTLEvent*ttl){
 if(p.empty()) return;
 ++paths_seen_;
 auto key=std::make_pair(src,p);auto it=paths_.find(key);
 if(it==paths_.end()) {
    PathSnapshot x;x.source=src;x.path=p;
    if(!config_.full_paths&&paths_.size()>=config_.top_n) {
        if(!config_.heavy_hitters || !config_.top_n){++paths_dropped_;++other_events_;return;}
        auto victim=std::min_element(paths_.begin(),paths_.end(),[](const auto& a,const auto& b){return a.second.estimated_total<b.second.estimated_total;});
        x.error_bound=x.estimated_total=victim->second.estimated_total;
        other_events_+=victim->second.total;paths_dropped_+=victim->second.total;
        paths_.erase(victim);
    }
    it=paths_.emplace(key,std::move(x)).first;
 }
 auto&e=it->second;++e.total;++e.estimated_total;if(out)++e.outcomes[static_cast<size_t>(*out)];if(ttl)++e.ttl_events[static_cast<size_t>(*ttl)];if(err)++e.errors[err];
}
void ProfilerCore::complete(size_t op,uint64_t ns,ProfileOutcome outcome,int error,const std::string&path,ProfileSource src){std::lock_guard<std::mutex>g(mutex_);if(op>=operations_.size())return;
if(size_t(outcome)>=size_t(ProfileOutcome::count) || size_t(src)>=size_t(ProfileSource::count))throw std::invalid_argument("invalid profiler outcome/source");
auto&o=operations_[op];
if(o.count==UINT64_MAX || UINT64_MAX-o.cumulative_ns<ns || (!path.empty() && paths_seen_==UINT64_MAX))throw std::overflow_error("profiler completion counters");
++o.count;o.cumulative_ns+=ns;o.latency.histogram.observe(ns);++o.outcomes[size_t(outcome)];++o.sources[size_t(src)];if(error)++o.errors[error];if(o.count==1){o.latency.min_ns=o.latency.max_ns=ns;}else{o.latency.min_ns=std::min(o.latency.min_ns,ns);o.latency.max_ns=std::max(o.latency.max_ns,ns);}auto&s=samples_[op];if(s.size()<config_.latency_capacity)s.push_back(ns);else if(config_.latency_capacity){std::uniform_int_distribution<uint64_t>d(0,o.count-1);auto j=d(rng_);if(j<config_.latency_capacity)s[size_t(j)]=ns;}record_path_locked(path,src,&outcome,error,nullptr);}
void ProfilerCore::record_ttl(const std::string&p,MetadataTTLEvent e){std::lock_guard<std::mutex>g(mutex_);
if(size_t(e)>=size_t(MetadataTTLEvent::count))throw std::invalid_argument("invalid TTL event");
if(md_ttl_totals_[size_t(e)]==UINT64_MAX || (!p.empty()&&paths_seen_==UINT64_MAX))throw std::overflow_error("profiler TTL counters");
++md_ttl_totals_[size_t(e)];record_path_locked(p,ProfileSource::metadata,nullptr,0,&e);}
ProfilerSnapshot ProfilerCore::finalize(RawSnapshot r){for(size_t i=0;i<r.snapshot.operations.size();++i){auto&o=r.snapshot.operations[i];auto&l=o.latency;l.count=o.count;l.sample_count=r.samples[i].size();l.cumulative_ns=o.cumulative_ns;std::sort(r.samples[i].begin(),r.samples[i].end());auto&s=r.samples[i];l.p50_ns=quantile(s,.5);l.p90_ns=quantile(s,.9);l.p95_ns=quantile(s,.95);l.p99_ns=quantile(s,.99);l.p999_ns=quantile(s,.999);}std::sort(r.snapshot.path_events.begin(),r.snapshot.path_events.end(),[](auto&a,auto&b){return a.estimated_total>b.estimated_total||(a.estimated_total==b.estimated_total&&std::tie(a.source,a.path)<std::tie(b.source,b.path));});return std::move(r.snapshot);}
ProfilerSnapshot ProfilerCore::snapshot()const{RawSnapshot r;{std::lock_guard<std::mutex>g(mutex_);r.snapshot.reset_epoch=reset_epoch_;r.snapshot.operations=operations_;r.samples=samples_;for(auto&[k,v]:paths_)r.snapshot.path_events.push_back(v);r.snapshot.path_mode=config_.full_paths?"full-path":!config_.top_n?"aggregate-only":config_.heavy_hitters?"bounded-heavy-hitter":"bounded-admitted-set";r.snapshot.paths_seen=paths_seen_;r.snapshot.paths_retained=paths_.size();r.snapshot.paths_dropped=paths_dropped_;r.snapshot.other_events=other_events_;r.snapshot.md_ttl_totals=md_ttl_totals_;}return finalize(std::move(r));}
ProfilerSnapshot ProfilerCore::snapshot_and_reset(){RawSnapshot r;{std::lock_guard<std::mutex>g(mutex_);r.snapshot.semantics="interval";r.snapshot.reset_epoch=reset_epoch_++;r.snapshot.operations.swap(operations_);r.samples.swap(samples_);for(auto&[k,v]:paths_)r.snapshot.path_events.push_back(v);r.snapshot.path_mode=config_.full_paths?"full-path":!config_.top_n?"aggregate-only":config_.heavy_hitters?"bounded-heavy-hitter":"bounded-admitted-set";r.snapshot.paths_seen=paths_seen_;r.snapshot.paths_retained=paths_.size();r.snapshot.paths_dropped=paths_dropped_;r.snapshot.other_events=other_events_;r.snapshot.md_ttl_totals=md_ttl_totals_;operations_.resize(r.snapshot.operations.size());samples_.resize(r.samples.size());paths_.clear();paths_seen_=paths_dropped_=other_events_=0;md_ttl_totals_.fill(0);}return finalize(std::move(r));}
ReportResult write_snapshot_bundle_atomic(const std::filesystem::path&dir,const std::string&host,uint64_t pid,uint64_t seq,const std::string&kind,const ProfilerSnapshot&s,const ProfilerConfig&c){ReportResult r;std::error_code ec;std::filesystem::create_directories(dir,ec);if(ec){r.error=ec.message();return r;}auto stem=filename_component(host)+"-"+std::to_string(pid)+"-"+std::to_string(seq)+"-"+filename_component(kind);auto manifest=dir/(stem+"-manifest.json");
if(std::filesystem::exists(manifest,ec)){r.error="bundle identity already published";return r;}
// Serialize writers of this identity; a leftover claim after a crash fails explicitly.
auto claim=dir/(stem+".publishing");
if(!std::filesystem::create_directory(claim,ec)){r.error="bundle identity publication claimed: "+ec.message();return r;}
struct ClaimCleanup { std::filesystem::path path; ~ClaimCleanup(){std::error_code ignored;std::filesystem::remove(path,ignored);} } cleanup{claim};
if(std::filesystem::exists(manifest,ec)){r.error="bundle identity already published";return r;}
std::ostringstream ops;ops<<"operation,total_calls,cumulative_latency_ns,min_ns,p50_ns,p90_ns,p95_ns,p99_ns,p999_ns,max_ns,sample_count,sampling_method,cache_hits,cache_misses,negative_results,source_none,source_metadata,source_data,source_tree,source_bypass,errors\n";uint64_t calls=0,hits=0,misses=0,neg=0,ns=0;for(size_t i=0;i<s.operations.size();++i){auto&o=s.operations[i];if(!o.count)continue;if(UINT64_MAX-calls<o.count || UINT64_MAX-ns<o.cumulative_ns || UINT64_MAX-hits<o.outcomes[1] || UINT64_MAX-misses<o.outcomes[2] || UINT64_MAX-neg<o.outcomes[3]){r.error="report aggregate counter overflow";return r;}
calls+=o.count;hits+=o.outcomes[1];misses+=o.outcomes[2];neg+=o.outcomes[3];ns+=o.cumulative_ns;ops<<(i<size_t(OperationFunction::size)?OperationFunctionNames[i]:"unknown")<<','<<o.count<<','<<o.cumulative_ns<<','<<o.latency.min_ns<<','<<o.latency.p50_ns<<','<<o.latency.p90_ns<<','<<o.latency.p95_ns<<','<<o.latency.p99_ns<<','<<o.latency.p999_ns<<','<<o.latency.max_ns<<','<<o.latency.sample_count<<','<<o.latency.sampling_method<<','<<o.outcomes[1]<<','<<o.outcomes[2]<<','<<o.outcomes[3]<<','<<o.sources[0]<<','<<o.sources[1]<<','<<o.sources[2]<<','<<o.sources[3]<<','<<o.sources[4]<<','<<csv_escape(errors_string(o.errors))<<'\n';}
std::ostringstream paths;paths<<"source,path,total_events,success,cache_hits,cache_misses,negative_results,ttl_stores,ttl_serves,ttl_expires,ttl_clears,errors,estimated_total,error_bound\n";for(auto&e:s.path_events)paths<<source_name(e.source)<<','<<csv_escape(e.path)<<','<<e.total<<','<<e.outcomes[0]<<','<<e.outcomes[1]<<','<<e.outcomes[2]<<','<<e.outcomes[3]<<','<<e.ttl_events[0]<<','<<e.ttl_events[1]<<','<<e.ttl_events[2]<<','<<e.ttl_events[3]<<','<<csv_escape(errors_string(e.errors))<<','<<e.estimated_total<<','<<e.error_bound<<'\n';
std::ostringstream agg;agg<<"metric,value\n"<<"total_counted_fuse_operations,"<<calls<<'\n'<<"total_cache_hits,"<<hits<<'\n'<<"total_cache_misses,"<<misses<<'\n'<<"total_negative_results,"<<neg<<'\n'<<"cumulative_measured_latency_ns,"<<ns<<'\n'<<"paths_seen,"<<s.paths_seen<<'\n'<<"paths_retained,"<<s.paths_retained<<'\n'<<"paths_dropped,"<<s.paths_dropped<<'\n'<<"other_events,"<<s.other_events<<'\n'<<"md_enoent_ttl_stores,"<<s.md_ttl_totals[0]<<'\n'<<"md_enoent_ttl_serves,"<<s.md_ttl_totals[1]<<'\n'<<"md_enoent_ttl_expires,"<<s.md_ttl_totals[2]<<'\n'<<"md_enoent_ttl_clears,"<<s.md_ttl_totals[3]<<'\n';
std::ostringstream histogram;
histogram << "operation,bucket_index,lower_bound_ns,upper_bound_ns,count\n";
for(size_t i=0;i<s.operations.size();++i) if(s.operations[i].count) {
    for(size_t b=0;b<LatencyHistogram::bucket_count;++b)
        histogram << (i<size_t(OperationFunction::size)?OperationFunctionNames[i]:"unknown") << ',' << b << ','
                  << LatencyHistogram::lower_bound(b) << ',' << LatencyHistogram::upper_bound(b) << ','
                  << s.operations[i].latency.histogram.counts[b] << '\n';
}
const std::array<std::pair<std::string,std::string>,4> data={{{"profiling_operations.csv",ops.str()},
    {"profiling_top_paths.csv",paths.str()},{"profiling_aggregate.csv",agg.str()},
    {"profiling_histogram.csv",histogram.str()}}};
for(const auto& x:data){auto path=dir/(stem+"-"+x.first);if(!atomic_write(path,x.second,r.error))return r;r.files.push_back(path.string());}
const auto identity=[](const char* key){const char* value=std::getenv(key);return value&&*value?"\""+json_escape(value)+"\"":std::string("null");};
auto now=std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
std::ostringstream m;
m << "{\n  \"schema_version\": 3,\n  \"sequence\": " << seq
  << ",\n  \"kind\": \"" << json_escape(kind) << "\",\n  \"timestamp_unix_ms\": " << now
  << ",\n  \"host\": \"" << json_escape(host) << "\",\n  \"pid\": " << pid
  << ",\n  \"run_id\": " << identity("COPPER_RUN_ID")
  << ",\n  \"daemon_generation\": " << identity("COPPER_DAEMON_GENERATION")
  << ",\n  \"semantics\": \"" << json_escape(s.semantics) << "\",\n  \"reset_epoch\": " << s.reset_epoch
  << ",\n  \"mode\": \"" << s.path_mode
  << "\",\n  \"config\": {\"top_n\": " << c.top_n << ", \"latency_capacity\": " << c.latency_capacity
  << ", \"sampling_method\": \"reservoir_algorithm_r\"},\n"
  << "  \"histogram\": {\"method\": \"fixed_log2_integer\", \"sample_provenance\": \"all_completions\", \"bucket_count\": 65, \"bounds\": \"inclusive\", \"unit\": \"ns\"},\n"
  << "  \"paths_seen\": " << s.paths_seen << ", \"paths_retained\": " << s.paths_retained
  << ", \"paths_dropped\": " << s.paths_dropped << ", \"other_events\": " << s.other_events << ",\n  \"files\": [";
for(size_t i=0;i<data.size();++i){if(i)m<<", ";m<<'"'<<json_escape(stem+"-"+data[i].first)<<'"';}
m << "],\n  \"file_integrity\": {";
for(size_t i=0;i<data.size();++i){if(i)m<<',';m << "\n    \"" << json_escape(stem+"-"+data[i].first)
    << "\": {\"sha256\": \"" << profiler_sha256(data[i].second) << "\", \"size_bytes\": " << data[i].second.size() << '}';}
m << "\n  }\n}\n";
auto mp=dir/(stem+"-manifest.json");if(!atomic_write(mp,m.str(),r.error))return r;
r.manifest_path=mp.string();r.files.push_back(r.manifest_path);r.success=true;return r;}
