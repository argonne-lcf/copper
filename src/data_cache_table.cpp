#include "data_cache_table.h"

void DataCacheTable::put_force(Key path, Value data_inode) {
    LOG(DEBUG) << "data_cache_table adding key: " << path << std::endl;
    data_cache[path] = data_inode;
}

std::optional<DataCacheTable::Value> DataCacheTable::get(Key path) {
    try {
        LOG(DEBUG) << "data_cache_table searching for key: " << path << std::endl;
        auto res = data_cache.at(path);
        LOG(DEBUG) << "data_cache_table found key: " << path << std::endl;
        return res;
    } catch(std::out_of_range e) {
        LOG(DEBUG) << "data_cache_table key not found: " << path << std::endl;
        return std::nullopt;
    }
}

void DataCacheTable::log() {
    LOG(DEBUG) << "data_cache_table has " << data_cache.size() << " "
               << ((data_cache.size() == 1) ? std::string("entry") : std::string("entries")) << std::endl;
    auto entry_num = 1;
    for(auto const& pair : data_cache) {
        LOG(DEBUG) << "data_cache_table entry #" << entry_num << std::endl;
        log_key_value(pair.first, pair.second);
        entry_num++;
    }
}

void DataCacheTable::log_key_value(Key path, Value data_inode) {
    LOG(DEBUG) << "data_cache_table key: " + path << std::endl;
    log_value(data_inode);
}

void DataCacheTable::log_value(Value data_inode) {
    LOG(DEBUG) << "\tdata byte size: " << std::to_string(data_inode.first.size()) << std::endl;
    LOG(DEBUG) << "\tinode number: " << std::to_string(data_inode.second) << std::endl;
}
