#include "md_cache_table.h"

void MDCacheTable::put_force(std::string key, struct stat* st) {
    lwlog_debug("adding key: %s", key.c_str());
    md_cache[key] = st;
}

std::optional<struct stat*> MDCacheTable::get(std::string key) {
    try {
        lwlog_debug("searching for key: %s", key.c_str());
        auto res = md_cache.at(key);
        lwlog_debug("key: %s found", key.c_str());
        return res;
    } catch(std::out_of_range e) {
        lwlog_debug("key: %s not found", key.c_str());
        return std::nullopt;
    }
}

void MDCacheTable::log() {
    lwlog_debug("logging md_cache");
#if(LOG_LEVEL == 7)
    std::cout << "md_cache has " << md_cache.size() << " "
              << ((md_cache.size() == 1) ? std::string("entry") : std::string("entries"))
              << std::endl;
#endif
    auto entry_num = 1;
    for(auto const& pair: md_cache) {
        std::cout << "entry #" << entry_num << std::endl;
        log_key_value(pair.first, pair.second);
        entry_num++;
    }
}

void MDCacheTable::log_key_value(std::string key, struct stat* st) {
    std::cout << "key: " + key << std::endl;
    log_value(st);
}

void MDCacheTable::log_value(struct stat* st) {
#if(LOG_LEVEL == 7)
    std::string stat_string = "stat structure:\n";

    stat_string += "\tst_dev: " +  std::to_string(st->st_dev) + "\n";
    stat_string += "\tst_ino: " +  std::to_string(st->st_ino) + "\n";
    stat_string += "\tst_mode: " +  std::to_string(st->st_mode) + "\n";
    stat_string += "\tst_nlink: " +  std::to_string(st->st_nlink) + "\n";
    stat_string += "\tst_uid: " +  std::to_string(st->st_uid) + "\n";
    stat_string += "\tst_gid: " +  std::to_string(st->st_gid) + "\n";
    stat_string += "\tst_rdev: " +  std::to_string(st->st_rdev) + "\n";
    stat_string += "\tst_size: " +  std::to_string(st->st_size) + "\n";
    stat_string += "\tst_atime: " +  std::to_string(st->st_atime) + "\n";
    stat_string += "\tst_mtime: " +  std::to_string(st->st_mtime) + "\n";
    stat_string += "\tst_ctime: " +  std::to_string(st->st_ctime) + "\n";
    stat_string += "\tst_blksize: " +  std::to_string(st->st_blksize) + "\n";
    stat_string += "\tst_blocks: " +  std::to_string(st->st_blocks) + "\n";

    std::cout << stat_string;
#endif
}
