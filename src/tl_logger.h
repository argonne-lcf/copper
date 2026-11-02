#include <thallium.hpp>
#include "aixlog.h"

#define COPPER_TRANSPORT_LOG_TAG "startup_transport"
#define COPPER_OPERATION_LOG_TAG "operation"

namespace tl = thallium;

class TLLogger: public tl::logger {
    public:

    void trace(const char* msg) const override {
        LOG(TRACE, COPPER_TRANSPORT_LOG_TAG) << "[THALLIUM] " << msg << std::endl;
    }

    void debug(const char* msg) const override {
        LOG(DEBUG, COPPER_TRANSPORT_LOG_TAG) << "[THALLIUM] " << msg << std::endl;
    }

    void info(const char* msg) const override {
        LOG(INFO, COPPER_TRANSPORT_LOG_TAG) << "[THALLIUM] " << msg << std::endl;
    }

    void warning(const char* msg) const override {
        LOG(WARNING, COPPER_TRANSPORT_LOG_TAG) << "[THALLIUM] " << msg << std::endl;
    }

    void error(const char* msg) const override {
        LOG(ERROR, COPPER_TRANSPORT_LOG_TAG) << "[THALLIUM] " << msg << std::endl;
    }

    void critical(const char* msg) const override {
        LOG(FATAL, COPPER_TRANSPORT_LOG_TAG) << "[THALLIUM] " << msg << std::endl;
    }
};