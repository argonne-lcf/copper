#include "startup/daemon_identity.h"
#include <sys/wait.h>
#include <iostream>
#include <filesystem>
#include <fstream>
#define CHECK(x) do { if(!(x)) { std::cerr << "CHECK failed: " #x << '\n'; return 1; } } while(false)
int main(int argc, char** argv) {
    if(argc == 4) {
        CopperStartup::publish_daemon_identity(argv[1], argv[3], "job.exact", "run-exact", "generation-exact", false);
        std::cout << getpid() << std::endl;
        sleep(30);
        return 0;
    }
    CHECK(CopperStartup::identity_exact_mount("/tmp/a b", "42 1 0:2 / /tmp/a\\040b rw - fuse.cu_fuse cu_fuse rw\n") == "{\"mount_id\":42,\"fstype\":\"fuse.cu_fuse\",\"source\":\"cu_fuse\"}");
    try { (void)CopperStartup::identity_exact_mount("/tmp/a", "42 1 0:2 / /tmp/a rw - fuse.cu_fuse cu_fuse rw\n43 1 0:2 / /tmp/a rw - fuse.cu_fuse cu_fuse rw\n"); CHECK(false); } catch(const std::runtime_error&) {}
    const auto dir = std::filesystem::temp_directory_path() / ("copper-identity-" + std::to_string(getpid()));
    std::filesystem::create_directory(dir);
    chmod(dir.c_str(), 0700);
    const auto parent = getpid();
    const auto child = fork();
    if(child == 0) {
        try { CopperStartup::publish_daemon_identity(dir, "/tmp/copper-test-mount", "job.exact", "run-exact", "generation-exact", false); _exit(0); }
        catch(...) { _exit(1); }
    }
    int status; waitpid(child, &status, 0); CHECK(WIFEXITED(status) && WEXITSTATUS(status) == 0);
    char host[256]{}; CHECK(gethostname(host, sizeof(host)) == 0);
    const auto file = dir / (std::string(host) + ".json");
    std::ifstream input(file); std::string text((std::istreambuf_iterator<char>(input)), {});
    CHECK(text.find("\"pid\":" + std::to_string(child)) != std::string::npos);
    CHECK(text.find("\"pid\":" + std::to_string(parent)) == std::string::npos);
    CHECK(text.find("\"starttime\":") != std::string::npos);
    CHECK(text.find("\"mount_identity\":null") != std::string::npos);
    struct stat st{}; CHECK(lstat(file.c_str(), &st) == 0); CHECK((st.st_mode & 0777) == 0600);
    std::filesystem::remove_all(dir);
    return 0;
}
// This executable intentionally has no FUSE dependency; fork proves publication uses the daemon's current PID.
