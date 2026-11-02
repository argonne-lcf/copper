#ifndef COPPER_STARTUP_TOOLS_H
#define COPPER_STARTUP_TOOLS_H

#include <algorithm>
#include <charconv>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <fcntl.h>
#include <functional>
#include <mutex>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <unistd.h>
#include <vector>

namespace CopperStartup {

inline int parse_integer(std::string_view text, const std::string& option) {
    int value{};
    const auto result = std::from_chars(text.data(), text.data() + text.size(), value);
    if(result.ec != std::errc{} || result.ptr != text.data() + text.size()) {
        throw std::invalid_argument(option + " must be an integer");
    }
    return value;
}

inline int parse_log_level(std::string_view text, const std::string& category) {
    const int value = parse_integer(text, category + " log level");
    if(value < 0 || value > 5) {
        throw std::invalid_argument(category + " log level must be in range 0-5");
    }
    return value;
}

inline int parse_positive_timeout_ms(std::string_view text, const std::string& name) {
    const int value = parse_integer(text, name + " timeout");
    if(value <= 0) {
        throw std::invalid_argument(name + " timeout must be greater than zero");
    }
    return value;
}

enum class LogCategory { startup, transport, operation };

inline bool category_enabled(LogCategory category, int startup_level, int operation_level, int message_level) {
    const int configured = category == LogCategory::operation ? operation_level : startup_level;
    return configured > 0 && message_level <= configured;
}

enum class PhaseState { begin, complete, failed };

inline const char* phase_state_name(PhaseState state) {
    switch(state) {
    case PhaseState::begin: return "begin";
    case PhaseState::complete: return "complete";
    case PhaseState::failed: return "failed";
    }
    return "failed";
}

inline std::string json_escape(std::string_view input) {
    std::string escaped;
    for(char ch : input) {
        switch(ch) {
        case '\\': escaped += "\\\\"; break;
        case '"': escaped += "\\\""; break;
        case '\n': escaped += "\\n"; break;
        case '\r': escaped += "\\r"; break;
        case '\t': escaped += "\\t"; break;
        default: escaped += ch; break;
        }
    }
    return escaped;
}

class StartupTelemetry {
    public:
    StartupTelemetry(std::string path, std::string host, long long pid)
    : path_(std::move(path)), host_(std::move(host)), pid_(pid), started_(std::chrono::steady_clock::now()) {}

    void record(std::string_view phase, PhaseState state, std::string_view detail = {}) {
        std::lock_guard lock(mutex_);
        const auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now() - started_).count();
        records_.push_back("{\"host\":\"" + json_escape(host_) + "\",\"pid\":" + std::to_string(pid_) +
                           ",\"phase\":\"" + json_escape(phase) + "\",\"state\":\"" + phase_state_name(state) +
                           "\",\"elapsed_us\":" + std::to_string(elapsed) + ",\"detail\":\"" +
                           json_escape(detail) + "\"}");
        persist();
    }

    const std::string& path() const { return path_; }

    private:
    void persist() const {
        const std::filesystem::path final_path(path_);
        if(final_path.has_parent_path()) std::filesystem::create_directories(final_path.parent_path());
        const std::filesystem::path temp_path = final_path.string() + ".tmp";
        {
            std::ofstream output(temp_path, std::ios::trunc);
            if(!output) throw std::runtime_error("unable to open startup telemetry temp file: " + temp_path.string());
            for(const auto& record : records_) output << record << '\n';
            output.flush();
            if(!output) throw std::runtime_error("unable to write startup telemetry temp file: " + temp_path.string());
        }
        const int fd = ::open(temp_path.c_str(), O_RDONLY);
        if(fd < 0) throw std::system_error(errno, std::generic_category(), "open startup telemetry temp file");
        if(::fsync(fd) != 0) {
            const int saved_errno = errno;
            ::close(fd);
            throw std::system_error(saved_errno, std::generic_category(), "fsync startup telemetry temp file");
        }
        ::close(fd);
        std::filesystem::rename(temp_path, final_path);
        if(final_path.has_parent_path()) {
            const int dir_fd = ::open(final_path.parent_path().c_str(), O_RDONLY | O_DIRECTORY);
            if(dir_fd >= 0) {
                ::fsync(dir_fd);
                ::close(dir_fd);
            }
        }
    }

    std::string path_;
    std::string host_;
    long long pid_;
    std::chrono::steady_clock::time_point started_;
    mutable std::mutex mutex_;
    std::vector<std::string> records_;
};

struct ReadinessHooks {
    std::function<bool()> probe;
    std::function<std::chrono::steady_clock::time_point()> now;
    std::function<void(std::chrono::milliseconds)> sleep_for;
};

struct ReadinessResult {
    bool ready{false};
    bool timed_out{false};
    int attempts{0};
    std::chrono::milliseconds elapsed{0};
    std::string error;
};

inline ReadinessResult wait_until_ready(const ReadinessHooks& hooks,
                                        std::chrono::milliseconds timeout,
                                        std::chrono::milliseconds initial_retry,
                                        std::chrono::milliseconds maximum_retry) {
    ReadinessResult result;
    const auto start = hooks.now();
    auto retry = initial_retry;
    while(true) {
        ++result.attempts;
        try {
            if(hooks.probe()) {
                result.ready = true;
                result.elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(hooks.now() - start);
                return result;
            }
        } catch(const std::exception& ex) {
            result.elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(hooks.now() - start);
            result.error = ex.what();
            return result;
        }

        result.elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(hooks.now() - start);
        if(result.elapsed >= timeout) {
            result.timed_out = true;
            return result;
        }
        const auto remaining = timeout - result.elapsed;
        hooks.sleep_for(std::min(retry, remaining));
        retry = std::min(maximum_retry, retry * 2);
    }
}

} // namespace CopperStartup

#endif
