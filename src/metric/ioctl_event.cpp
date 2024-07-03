#include "ioctl_event.h"

std::ostream& IoctlEvent::log_ioctl_event(std::ostream& os) {
    os << "Ioctl Events"
       << " {" << std::endl;

    for(const auto& entry : ioctl_cache_event_table) {
        os << entry.first << " {" << std::endl;
        for(const auto& cmd : entry.second) {
            os << "cmd: " << cmd << std::endl;
        }
        os << "}" << std::endl;
    }
    os << "}";

    return os;
}

void IoctlEvent::record_ioctl_event(const std::string& path, int cmd) {
    auto& cmds = ioctl_cache_event_table[path];
    cmds.push_back(cmd);
}
