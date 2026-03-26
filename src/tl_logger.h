#include <thallium.hpp>
#include "aixlog.h"

namespace tl = thallium;

class TLLogger: public tl::logger {
    public:

    void trace(const char* msg) const override {
        LOG(TRACE) << "[THALLIUM] " << msg << std::endl;
    }

    void debug(const char* msg) const override {
        LOG(DEBUG) << "[THALLIUM] " << msg << std::endl;
    }

    void info(const char* msg) const override {
        LOG(INFO) << "[THALLIUM] " << msg << std::endl;
    }

    void warning(const char* msg) const override {
        LOG(WARNING) << "[THALLIUM] " << msg << std::endl;
    }

    void error(const char* msg) const override {
        LOG(ERROR) << "[THALLIUM] " << msg << std::endl;
    }

    void critical(const char* msg) const override {
        LOG(FATAL) << "[THALLIUM] " << msg << std::endl;
    }
};