#ifndef CU_FUSE_IOCTL_EVENT_H
#define CU_FUSE_IOCTL_EVENT_H

#include <unordered_map>
#include <string>

#include "../fs/util.h"

class IoctlEvent {
    public:
    static void record_ioctl_event(const std::string& path, int cmd);

    static std::ostream& log_ioctl_event(std::ostream& os);

    static void reset_ioctl_event() {
        ioctl_cache_event_table.clear();
    }

    // NOTE: pair.first is path_string
    //       pair.second ioctl cmd
    static inline std::unordered_map<std::string, std::vector<int>> ioctl_cache_event_table;
};

#endif // CU_FUSE_IOCTL_EVENT_H