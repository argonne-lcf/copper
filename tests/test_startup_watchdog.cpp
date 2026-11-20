// Fault injection for the startup watchdog's emergency termination (review r5
// finding 1). Each case runs in a forked child that starts a watchdog thread
// calling watchdog_terminate(); the parent checks the child exits with the
// watchdog's exit code within a bound, or (default SIGTERM action) is killed
// by SIGTERM. A child still alive at the bound is SIGKILLed and is a failure.
//
// Cases:
//  blocked_handler   SIGTERM handler blocks forever (reviewer's reproduction)
//  ignored_sigterm   SIGTERM is SIG_IGN
//  all_threads_mask  every other thread blocks SIGTERM (signal stays pending)
//  blocked_report    the diagnostic report never returns (telemetry stuck) + stalling handler
//  busy_phase_lock   the report needs a lock held forever by the stuck path + stalling handler
//  default_action    no handler: SIGTERM terminates the process (launcher sees it)
#include "startup/startup_watchdog.h"

#include <cerrno>
#include <chrono>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <mutex>
#include <string>
#include <sys/wait.h>
#include <thread>
#include <unistd.h>

static constexpr int kExit = ETIMEDOUT;
static std::timed_mutex g_phase_lock;

static void stall_handler(int) {
    const char m[] = "  child: entered SIGTERM handler; stalling\n";
    (void)!write(2, m, sizeof m - 1);
    while(true) pause();
}

static void child(const std::string& mode) {
    // blocked_report / busy_phase_lock also install the stalling handler, so a
    // pass requires the _Exit fallback after a stuck report, not just the
    // default SIGTERM action.
    if(mode == "blocked_handler" || mode == "blocked_report" || mode == "busy_phase_lock")
        std::signal(SIGTERM, stall_handler);
    if(mode == "ignored_sigterm") std::signal(SIGTERM, SIG_IGN);
    if(mode == "all_threads_mask") {
        sigset_t s; sigemptyset(&s); sigaddset(&s, SIGTERM);
        pthread_sigmask(SIG_BLOCK, &s, nullptr);  // inherited by threads created below
    }
    std::function<void()> report = [] {};
    if(mode == "blocked_report") report = [] { while(true) std::this_thread::sleep_for(std::chrono::seconds(1)); };
    if(mode == "busy_phase_lock") {
        g_phase_lock.lock();  // the "stuck startup path" holds it forever
        report = [] { std::lock_guard<std::timed_mutex> g(g_phase_lock); };
    }
    std::thread wd([report] {
        CopperStartup::watchdog_terminate(report, std::chrono::milliseconds(300), std::chrono::milliseconds(500), kExit);
    });
    wd.detach();
    // main thread = the stuck startup path
    while(true) pause();
}

static bool run_case(const std::string& mode) {
    const auto t0 = std::chrono::steady_clock::now();
    const pid_t pid = fork();
    if(pid == 0) { child(mode); _exit(99); }
    int status = 0;
    pid_t r = 0;
    while((r = waitpid(pid, &status, WNOHANG)) == 0) {
        if(std::chrono::steady_clock::now() - t0 > std::chrono::seconds(5)) {
            kill(pid, SIGKILL);
            waitpid(pid, &status, 0);
            std::printf("FAIL %-17s child still alive after 5 s (SIGKILLed)\n", mode.c_str());
            return false;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    const long long ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - t0).count();
    const bool exited_wd = WIFEXITED(status) && WEXITSTATUS(status) == kExit;
    const bool sigterm = WIFSIGNALED(status) && WTERMSIG(status) == SIGTERM;
    const bool ok = (mode == "default_action") ? (sigterm || exited_wd) : exited_wd;
    std::printf("%s %-17s %s after %lld ms\n", ok ? "ok  " : "FAIL", mode.c_str(),
                WIFEXITED(status) ? ("exit " + std::to_string(WEXITSTATUS(status))).c_str()
                                  : ("signal " + std::to_string(WTERMSIG(status))).c_str(), ms);
    return ok && ms < 3000;
}

int main() {
    bool ok = true;
    for(const char* m : {"blocked_handler", "ignored_sigterm", "all_threads_mask", "blocked_report", "busy_phase_lock", "default_action"})
        ok = run_case(m) && ok;
    if(!ok) { std::printf("FAIL: startup watchdog fault injection\n"); return 1; }
    std::printf("PASS: startup watchdog terminates under all 6 injected faults\n");
    return 0;
}
