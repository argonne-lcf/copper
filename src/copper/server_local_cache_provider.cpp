#include "server_local_cache_provider.h"
#include <cassert>

void ServerLocalCacheProvider::rpcLstat(const tl::request& req, const bool dest, const std::string& path_string) const {
    const auto& req_from_addr = static_cast<std::string>(req.get_endpoint());
    const auto& my_curr_node_addr = static_cast<std::string>(get_engine().self());

    LOG(INFO, RPC_METADATA_TAG) << "my address:" << my_curr_node_addr << std::endl;
    LOG(INFO, RPC_METADATA_TAG) << "req_coming_from_addr:" << req_from_addr << ", path_string: " << path_string << std::endl;

    lstat_return_type lstat_response;
    bool cached = false;
    const auto entry_opt = CacheTables::md_cache_table.get(path_string);

    if(entry_opt.has_value()) {
        LOG(INFO, RPC_METADATA_TAG) << "found in local cache" << std::endl;
        lstat_response = lstat_return_type(Constants::fs_operation_success, entry_opt.value()->get_vec());
        cached = true;
    } else if(Node::root->addr == my_curr_node_addr) {
        LOG(INFO, RPC_METADATA_TAG) << "requesting data from underlying filesystem" << std::endl;
        CuStat cu_stat;
        if(lstat(path_string.c_str(), cu_stat.get_st()) == -1) {
            LOG(ERROR) << "storage stat for path: " << path_string << " returned with errno: " << errno << std::endl;
            lstat_response = lstat_return_type(-errno, std::vector<std::byte>(0));
        } else {
            lstat_response = lstat_return_type(Constants::fs_operation_success, cu_stat.get_vec());
        }
    } else {
        assert(Node::parent_addr.has_value());
#ifdef BLOCK_REDUNDANT_RPCS
        // NOTE: were the first one to make rpc request
        if(!CacheTables::md_path_status_cache_table.check_and_put_force(path_string)) {
            LOG(INFO, RPC_METADATA_TAG) << "first to request path_string" << std::endl;
#endif
            LOG(INFO, RPC_METADATA_TAG) << "going to parent " << Node::parent_addr.value() << std::endl;
            lstat_response = rpc_lstat.on(get_engine().lookup(Node::parent_addr.value()))(false, path_string);
            LOG(INFO, RPC_METADATA_TAG) << "returned from parent " << Node::parent_addr.value() << std::endl;
#ifdef BLOCK_REDUNDANT_RPCS
        } else { // NOTE: request already made wait for cache to be filled
            LOG(INFO, RPC_METADATA_TAG) << " not first to request path_string, waiting on cache entry..." << std::endl;
            int status = CacheTables::md_path_status_cache_table.wait_on_cache_status(path_string);

            if(status != 0) {
                lstat_response = std::make_pair(status, std::vector<std::byte>(0));
            } else {
                assert(CacheTables::md_cache_table.get(path_string).has_value());
                lstat_response = std::make_pair(status, CacheTables::md_cache_table.get(path_string).value()->get_vec());
            }

            cached = true;
        }
#endif
    }

    if(!cached && lstat_response.first == Constants::fs_operation_success) {
        Metric::stat_inter_cache_miss++;
        LOG(INFO, RPC_METADATA_TAG) << "caching intermediate rpc_lstat_response" << std::endl;
        CacheTables::md_cache_table.put_force(path_string, CuStat{lstat_response.second});
    } else if(cached) {
        Metric::stat_inter_cache_hit++;
        LOG(INFO, RPC_METADATA_TAG) << "lstat response already cached" << std::endl;
    } else {
        LOG(INFO, RPC_METADATA_TAG) << "lstat response != 0, not caching intermediate rpc_lstat_response" << std::endl;
    }

#ifdef BLOCK_REDUNDANT_RPCS
    CacheTables::md_path_status_cache_table.update_cache_status(path_string, lstat_response.first);
#endif

    if(!dest) {
        TIME_RPC(req.respond(lstat_response));
    } else {
        // NOTE: final response so send status only
        LOG(INFO, RPC_METADATA_TAG) << "returning final response to client" << std::endl;
        TIME_RPC(req.respond(lstat_response.first));
    }
}

void ServerLocalCacheProvider::rpcRead(const tl::request& req, const bool dest, const std::string& path_string) const {
    const auto& req_from_addr = static_cast<std::string>(req.get_endpoint());
    const auto& my_curr_node_addr = static_cast<std::string>(get_engine().self());

    LOG(INFO, RPC_DATA_TAG) << "my address:" << my_curr_node_addr << std::endl;
    LOG(INFO, RPC_DATA_TAG) << "req_coming_from_addr: " << req_from_addr << ", path_string: " << path_string << std::endl;

    read_return_type read_response;
    bool cached = false;
    const auto entry_opt = CacheTables::data_cache_table.get(path_string);

    if(!dest && entry_opt.has_value()) {
        LOG(INFO, RPC_DATA_TAG) << "found in local cache" << std::endl;
        const auto bytes = entry_opt.value();
        read_response = std::make_pair(bytes->size(), *bytes);
        cached = true;
    } else if(Node::root->addr == my_curr_node_addr) {
        LOG(INFO, RPC_DATA_TAG) << "requesting data from underlying filesystem" << std::endl;
        try {
            const std::vector<std::byte>& file_bytes = Util::read_ent_file(path_string);
            read_response = std::make_pair(file_bytes.size(), file_bytes);
        } catch(std::exception& e) {
            LOG(WARNING, RPC_DATA_TAG) << e.what() << std::endl;
            read_response = std::make_pair(-Constants::fs_operation_error, std::vector<std::byte>(0));
        }
    } else {
        assert(Node::parent_addr.has_value());
#ifdef BLOCK_REDUNDANT_RPCS
        // NOTE: were the first one to make rpc request
        if(!CacheTables::data_path_status_cache_table.check_and_put_force(path_string)) {
            LOG(INFO, RPC_DATA_TAG) << "first to request path_string" << std::endl;
#endif
            LOG(INFO, RPC_DATA_TAG) << "going to parent " << Node::parent_addr.value() << std::endl;
            read_response = rpc_readfile.on(get_engine().lookup(Node::parent_addr.value() ))(false, path_string);
            LOG(INFO, RPC_DATA_TAG) << "returned from parent " << Node::parent_addr.value() << std::endl;
#ifdef BLOCK_REDUNDANT_RPCS
        } else {
            // NOTE: request already made wait for cache to be filled
            LOG(INFO, RPC_DATA_TAG) << " not first to request path_string, waiting on cache entry..." << std::endl;
            int status = CacheTables::data_path_status_cache_table.wait_on_cache_status(path_string);

            if(status < 0) {
                read_response = std::make_pair(status, std::vector<std::byte>(0));
            } else {
                assert(CacheTables::data_cache_table.get(path_string).has_value());
                read_response = std::make_pair(status, *CacheTables::data_cache_table.get(path_string).value());
            }

            cached = true;
        }
#endif
    }

    if(!cached && read_response.first >= 0) {
        Metric::read_inter_cache_miss++;
        LOG(INFO, RPC_READDIR_TAG) << "caching intermediate rpc_readfile_response" << std::endl;
        CacheTables::data_cache_table.put_force(path_string, std::vector<std::byte>(read_response.second));
    } else if(cached) {
        Metric::read_inter_cache_hit++;
        LOG(INFO, RPC_DATA_TAG) << "readfile response already cached" << std::endl;
    } else {
        LOG(INFO, RPC_DATA_TAG) << "readfile response < 0, not caching intermediate rpc_readfile_response" << std::endl;
    }

#ifdef BLOCK_REDUNDANT_RPCS
    CacheTables::data_path_status_cache_table.update_cache_status(path_string, read_response.first);
#endif

    if(!dest) {
        LOG(INFO, RPC_DATA_TAG) << "byte size: " << read_response.second.size() << std::endl;
        TIME_RPC(req.respond(read_response));
    } else {
        // NOTE: final response so send status only
        LOG(INFO, RPC_DATA_TAG) << "returning final response to client" << std::endl;
        TIME_RPC(req.respond(read_response.first));
    }
}

void ServerLocalCacheProvider::rpcReaddir(const tl::request& req, const bool dest, const std::string& path_string) const {
    const auto& req_from_addr = static_cast<std::string>(req.get_endpoint());
    const auto& my_curr_node_addr = static_cast<std::string>(get_engine().self());

    LOG(INFO, RPC_READDIR_TAG) << "my address:" << my_curr_node_addr << std::endl;
    LOG(INFO, RPC_READDIR_TAG) << "req_coming_from_addr: " << req_from_addr << ", path_string: " << path_string << std::endl;

    readdir_return_type readdir_response;
    bool cached = false;
    const auto entry_opt = CacheTables::tree_cache_table.get(path_string);

    if(!dest && entry_opt.has_value()) {
        LOG(INFO, RPC_READDIR_TAG) << "found in local cache" << std::endl;
        const auto paths = entry_opt.value();
        readdir_response = std::make_pair(Constants::fs_operation_success, *paths);
        cached = true;
    } else if(Node::root->addr == my_curr_node_addr) {
        LOG(INFO, RPC_READDIR_TAG) << "requesting data from underlying filesystem" << std::endl;
        auto entries = std::vector<std::string>{};
        DIR* dp = opendir(path_string.c_str());
        if(dp == nullptr) {
            LOG(WARNING) << "failed to passthrough readdir" << std::endl;
            LOG(ERROR) << "storage opendir for path: " << path_string << " returned with errno: " << errno << std::endl;
            readdir_response = std::make_pair(-errno, std::vector<std::string>(0));
        } else {
            // NOTE: when readdir returns nullptr it means either:
            //       1. end of directory
            //       2. errno was set to a nonzero value
            dirent* de; errno = 0;
            while((de = readdir(dp)) != nullptr) {
                entries.emplace_back(de->d_name); errno = 0;
            }

            if(errno != 0) {
                LOG(ERROR) << "storage readdir for path: " << path_string << " returned with errno: " << errno << std::endl;
                readdir_response = std::make_pair(-errno, std::vector<std::string>(0));
            } else {
                readdir_response = std::make_pair(Constants::fs_operation_success, entries);
            }

            closedir(dp);
        }
    } else {
        assert(Node::parent_addr.has_value());
#ifdef BLOCK_REDUNDANT_RPCS
        // NOTE: were the first one to make rpc request
        if(!CacheTables::tree_path_status_cache_table.check_and_put_force(path_string)) {
            LOG(INFO, RPC_READDIR_TAG) << "first to request path_string" << std::endl;
#endif
            LOG(INFO, RPC_READDIR_TAG) << "going to parent " << Node::parent_addr.value() << std::endl;
            readdir_response = rpc_readdir.on(get_engine().lookup(Node::parent_addr.value() ))(false, path_string);
            LOG(INFO, RPC_READDIR_TAG) << "returned from parent " << Node::parent_addr.value()  << std::endl;
#ifdef BLOCK_REDUNDANT_RPCS
        } else { // NOTE: request already made wait for cache to be filled
            LOG(INFO, RPC_READDIR_TAG) << "not first to request path_string, waiting on cache entry..." << std::endl;
            const auto status = CacheTables::tree_path_status_cache_table.wait_on_cache_status(path_string);

            if(status != Constants::fs_operation_success) {
                readdir_response = std::make_pair(status, std::vector<std::string>(0));
            } else {
                assert(CacheTables::tree_cache_table.get(path_string).has_value());
                readdir_response = std::make_pair(status, *CacheTables::tree_cache_table.get(path_string).value());
            }

            cached = true;
        }
#endif
    }

    if(!cached && readdir_response.first == Constants::fs_operation_success) {
        Metric::readdir_inter_cache_hit++;
        LOG(INFO, RPC_READDIR_TAG) << "caching intermediate rpc_readfile_response" << std::endl;
        CacheTables::tree_cache_table.put_force(path_string, std::vector<std::string>(readdir_response.second));
    } else if(cached) {
        Metric::readdir_inter_cache_miss++;
        LOG(INFO, RPC_READDIR_TAG) << "readdir response already cached" << std::endl;
    } else {
        LOG(INFO, RPC_READDIR_TAG) << "readdir response != 0, not caching intermediate rpc_readfile_response" << std::endl;
    }

#ifdef BLOCK_REDUNDANT_RPCS
    CacheTables::tree_path_status_cache_table.update_cache_status(path_string, readdir_response.first);
#endif

    if(!dest) {
        TIME_RPC(req.respond(readdir_response));
    } else {
        // NOTE: final response so send status only
        LOG(INFO, RPC_READDIR_TAG) << "returning final response to client" << std::endl;
        TIME_RPC(req.respond(readdir_response.first));
    }
}
