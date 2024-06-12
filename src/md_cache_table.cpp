#include "md_cache_table.h"

void MDCacheTable::put_force(Key path, Value st) {
    LOG(DEBUG) << "md_cache_table adding key: " << path << std::endl;
    md_cache[path] = st;
}


std::optional<MDCacheTable::Value> MDCacheTable::get(Key path) {
    try {
        LOG(DEBUG) << "md_cache_table searching for key: " << path << std::endl;
        auto res = md_cache.at(path);
        LOG(DEBUG) << "md_cache_table found key: " << path << std::endl;
        return res;
    } catch(std::out_of_range e) {
        LOG(DEBUG) << "md_cache_table key not found: " << path << std::endl;
        return std::nullopt;
    }
}

void MDCacheTable::log() {
    LOG(DEBUG) << "logging md_cache_table" << std::endl;
    std::cout << "md_cache_table has " << md_cache.size() << " "
              << ((md_cache.size() == 1) ? std::string("entry") : std::string("entries")) << std::endl;
    auto entry_num = 1;
    for(auto const& pair : md_cache) {
        LOG(DEBUG) << "md_cache_table entry #" << entry_num << std::endl;
        log_key_value(pair.first, pair.second);
        entry_num++;
    }
}

void MDCacheTable::log_key_value(Key path, Value st) {
    LOG(DEBUG) << "md_cache_table key: " + path << std::endl;
    log_value(st);
}

void MDCacheTable::log_value(Value st) {
    std::string stat_string = "md_cache_table stat structure:\n";

    stat_string += "\tst_dev: " + std::to_string(st->st_dev) + "\n";
    stat_string += "\tst_ino: " + std::to_string(st->st_ino) + "\n";
    stat_string += "\tst_mode: " + std::to_string(st->st_mode) + "\n";
    stat_string += "\tst_nlink: " + std::to_string(st->st_nlink) + "\n";
    stat_string += "\tst_uid: " + std::to_string(st->st_uid) + "\n";
    stat_string += "\tst_gid: " + std::to_string(st->st_gid) + "\n";
    stat_string += "\tst_rdev: " + std::to_string(st->st_rdev) + "\n";
    stat_string += "\tst_size: " + std::to_string(st->st_size) + "\n";
    stat_string += "\tst_atime: " + std::to_string(st->st_atime) + "\n";
    stat_string += "\tst_mtime: " + std::to_string(st->st_mtime) + "\n";
    stat_string += "\tst_ctime: " + std::to_string(st->st_ctime) + "\n";
    stat_string += "\tst_blksize: " + std::to_string(st->st_blksize) + "\n";
    stat_string += "\tst_blocks: " + std::to_string(st->st_blocks) + "\n";

    LOG(DEBUG) << stat_string;
}
