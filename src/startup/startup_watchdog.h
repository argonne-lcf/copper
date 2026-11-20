#ifndef COPPER_STARTUP_WATCHDOG_H
#define COPPER_STARTUP_WATCHDOG_H

// Emergency termination path for the startup watchdog (review r4 finding 3,
// r5 finding 1). Termination must not depend on anything the stuck startup
// path may hold or on how SIGTERM is handled:
//  * the diagnostic report runs on a detached helper with a bounded wait, so a
//    blocked telemetry mutex / fsync / stderr pipe cannot stop termination;
//  * the calling (watchdog) thread blocks SIGTERM for itself before sending a
//    process-directed SIGTERM, so a handler can never run on the watchdog
//    thread. If the handler blocks on another thread, if SIGTERM is ignored,
//    or if every other thread blocks it, the watchdog still reaches _Exit
//    after `grace`;
//  * std::_Exit takes no locks and runs no handlers.
// Call only from a dedicated watchdog thread; this function does not return.

#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdlib>
#include <memory>
#include <pthread.h>
#include <signal.h>
#include <thread>
#include <unistd.h>
#include <utility>

namespace CopperStartup {

template <typename Report>
[[noreturn]] inline void watchdog_terminate(Report&& report, std::chrono::milliseconds report_wait,
                                            std::chrono::milliseconds grace, int exit_code) {
    sigset_t term;
    sigemptyset(&term);
    sigaddset(&term, SIGTERM);
    pthread_sigmask(SIG_BLOCK, &term, nullptr);  // never run a SIGTERM handler on this thread

    try {
        auto reported = std::make_shared<std::atomic<bool>>(false);
        std::thread([reported, r = std::forward<Report>(report)]() mutable {
            r();
            reported->store(true, std::memory_order_release);
        }).detach();
        const auto report_end = std::chrono::steady_clock::now() + report_wait;
        while(!reported->load(std::memory_order_acquire) && std::chrono::steady_clock::now() < report_end)
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
    } catch(...) {
        // diagnostics are best-effort (e.g. thread creation failed); keep terminating
    }

    // Process-directed: delivered to some thread that does not block SIGTERM,
    // never this one. Lets FUSE/runtime teardown handlers detach the mount.
    ::kill(::getpid(), SIGTERM);
    std::this_thread::sleep_for(grace);
    std::_Exit(exit_code);
}

} // namespace CopperStartup

#endif // COPPER_STARTUP_WATCHDOG_H
