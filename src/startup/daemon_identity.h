#ifndef COPPER_DAEMON_IDENTITY_H
#define COPPER_DAEMON_IDENTITY_H
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <stdexcept>
#include <system_error>
#include <fcntl.h>
#include <pwd.h>
#include <sys/stat.h>
#include <unistd.h>

namespace CopperStartup {
inline std::string identity_json_string(const std::string& value) {
    const char* hex = "0123456789abcdef";
    std::string out = "\"";
    for(unsigned char c : value) {
        if(c == '"' || c == '\\') { out += '\\'; out += c; }
        else if(c < 32) { out += "\\u00"; out += hex[c >> 4]; out += hex[c & 15]; }
        else out += c;
    }
    return out + '"';
}
inline std::string identity_read(const std::string& path) {
    std::ifstream in(path); if(!in) throw std::runtime_error("identity read failed: " + path);
    std::ostringstream out; out << in.rdbuf(); return out.str();
}
inline std::string mountinfo_unescape(const std::string& value) {
    std::string out;
    for(size_t i=0; i<value.size(); ++i) {
        if(value[i]=='\\' && i+3<value.size() && value[i+1]>='0' && value[i+1]<='7' && value[i+2]>='0' && value[i+2]<='7' && value[i+3]>='0' && value[i+3]<='7') {
            out += static_cast<char>((value[i+1]-'0')*64+(value[i+2]-'0')*8+value[i+3]-'0'); i+=3;
        } else out += value[i];
    }
    return out;
}
inline std::string identity_exact_mount(const std::string& mount, const std::string& text) {
    std::istringstream lines(text); std::string line, found;
    while(std::getline(lines,line)) {
        const auto separator=line.find(" - "); if(separator==std::string::npos) throw std::runtime_error("invalid mountinfo");
        std::istringstream left(line.substr(0,separator)), right(line.substr(separator+3));
        std::string id,parent,device,root,point,fs,source;
        left>>id>>parent>>device>>root>>point; right>>fs>>source;
        if(mountinfo_unescape(point)!=mount) continue;
        if(!found.empty()) throw std::runtime_error("stacked mounts are ambiguous");
        if(fs.rfind("fuse",0)!=0) throw std::runtime_error("identity requires an exact FUSE mount");
        size_t parsed=0; auto numeric=std::stoull(id,&parsed); if(parsed!=id.size()) throw std::runtime_error("invalid mount ID");
        found="{\"mount_id\":"+std::to_string(numeric)+",\"fstype\":"+identity_json_string(fs)+",\"source\":"+identity_json_string(mountinfo_unescape(source))+"}";
    }
    return found.empty()?"null":found;
}
// The directory is created by the launcher, before fork. The process identity
// is captured only when called from the post-daemonization FUSE init callback.
inline void publish_daemon_identity(const std::filesystem::path& directory, const std::string& mount,
                                    const std::string& job, const std::string& run, const std::string& generation,
                                    bool require_mount=true) {
    if(job.empty() || run.empty() || generation.empty()) throw std::runtime_error("missing daemon run identity");
    if(!std::filesystem::path(mount).is_absolute() || mount=="/" || std::filesystem::path(mount).lexically_normal().string()!=mount) throw std::runtime_error("invalid daemon mount");
    const int dir=open(directory.c_str(),O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC);
    if(dir<0) throw std::system_error(errno,std::generic_category(),"open identity directory");
    struct Guard { int fd; ~Guard(){ if(fd>=0) close(fd); } } guard{dir};
    struct stat st{};
    if(fstat(dir,&st)!=0 || st.st_uid!=getuid() || (st.st_mode&077)!=0) throw std::runtime_error("identity directory must be caller-owned and private");
    char host[256]{}; if(gethostname(host,sizeof(host)-1)!=0) throw std::system_error(errno,std::generic_category(),"hostname");
    const std::string hostname(host);
    if(hostname.empty() || hostname.find_first_not_of("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789_.-")!=std::string::npos) throw std::runtime_error("unsafe hostname");
    const auto pid=getpid(); const auto uid=getuid(); const auto user=getpwuid(uid);
    if(!user) throw std::runtime_error("unable to resolve daemon user");
    const auto stat_text=identity_read("/proc/self/stat"); const auto end=stat_text.rfind(')');
    if(end==std::string::npos) throw std::runtime_error("invalid process stat");
    std::istringstream fields(stat_text.substr(end+1)); std::string start;
    for(int i=0;i<20;++i) if(!(fields>>start)) throw std::runtime_error("short process stat");
    auto boot=identity_read("/proc/sys/kernel/random/boot_id"); while(!boot.empty() && (boot.back()=='\n'||boot.back()=='\r')) boot.pop_back();
    const auto exe=std::filesystem::read_symlink("/proc/self/exe").string();
    const auto mount_identity=identity_exact_mount(mount,identity_read("/proc/self/mountinfo"));
    if(require_mount && mount_identity=="null") throw std::runtime_error("exact daemon mount is absent");
    const std::string bytes="{\"schema_version\":1,\"job_id\":"+identity_json_string(job)+",\"run_id\":"+identity_json_string(run)+",\"daemon_generation\":"+identity_json_string(generation)+",\"host\":"+identity_json_string(hostname)+",\"pid\":"+std::to_string(pid)+",\"uid\":"+std::to_string(uid)+",\"user\":"+identity_json_string(user->pw_name)+",\"starttime\":"+start+",\"boot_id\":"+identity_json_string(boot)+",\"exe\":"+identity_json_string(exe)+",\"mount\":"+identity_json_string(mount)+",\"mount_identity\":"+mount_identity+"}\n";
    const auto temporary=hostname+"."+std::to_string(pid)+".tmp";
    const int fd=openat(dir,temporary.c_str(),O_WRONLY|O_CREAT|O_EXCL|O_NOFOLLOW|O_CLOEXEC,0600);
    if(fd<0) throw std::system_error(errno,std::generic_category(),"create identity record");
    Guard file{fd};
    try {
        size_t done=0; while(done<bytes.size()) { auto n=write(fd,bytes.data()+done,bytes.size()-done); if(n<0 && errno==EINTR) continue; if(n<=0) throw std::system_error(errno?errno:EIO,std::generic_category(),"write identity"); done+=n; }
        if(fsync(fd)!=0) throw std::system_error(errno,std::generic_category(),"fsync identity");
        if(renameat(dir,temporary.c_str(),dir,(hostname+".json").c_str())!=0 || fsync(dir)!=0) throw std::system_error(errno,std::generic_category(),"publish identity");
    } catch(...) { unlinkat(dir,temporary.c_str(),0); throw; }
}
}
#endif
