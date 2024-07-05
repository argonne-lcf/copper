#include "server_local_cache_provider.h"
#include <cassert>

std::vector<std::pair<std::string, std::string>> ServerLocalCacheProvider::global_peer_pairs;
std::mutex ServerLocalCacheProvider::mtx;
std::vector<std::string> ServerLocalCacheProvider::node_address_data;
std::vector<tl::endpoint> ServerLocalCacheProvider::m_peers;

tl::remote_procedure ServerLocalCacheProvider::rpc_lstat;
tl::remote_procedure ServerLocalCacheProvider::rpc_readfile;
tl::remote_procedure ServerLocalCacheProvider::rpc_readdir;

void ServerLocalCacheProvider::rpcLstat(const tl::request& req, const bool dest, const std::string& path_string) const {
    const auto& req_from_addr = static_cast<std::string>(req.get_endpoint());
    const auto& my_curr_node_addr = static_cast<std::string>(get_engine().self());

    LOG(INFO, RPC_METADATA_TAG) << "my address:" << my_curr_node_addr << std::endl;
    LOG(INFO, RPC_METADATA_TAG) << "req_coming_from_addr:" << req_from_addr << ", path_string: " << path_string << std::endl;

    lstat_return_type lstat_response;
    bool cached = false;

    // NOTE: check if request can be resolved in local cache
    const auto entry_opt = CacheTables::md_cache_table.get(path_string);
    if(entry_opt.has_value()) {
        LOG(INFO, RPC_METADATA_TAG) << "found in local cache" << std::endl;
        lstat_response = lstat_return_type(Constants::fs_operation_success, entry_opt.value()->get_st_vec_cpy());
        cached = true;
    } else if(Node::root->data == my_curr_node_addr) {
        LOG(INFO, RPC_METADATA_TAG) << "requesting data from underlying filesystem" << std::endl;
        CuStat cu_stat;
        if(lstat(path_string.c_str(), cu_stat.get_st()) == -1) {
            lstat_response = lstat_return_type(-errno, std::vector<std::byte>(0));
        } else {
            lstat_response = lstat_return_type(Constants::fs_operation_success, cu_stat.get_move_st_vec());
        }
    } else {
        // NOTE: were the first one to make rpc request
        if(!CacheTables::md_path_status_cache_table.check_and_put_force(path_string)) {
            LOG(INFO, RPC_METADATA_TAG) << "first to request path_string" << std::endl;
            std::string parentofmynode;
            NodeTree::getParentfromtree(Node::root, my_curr_node_addr, parentofmynode);
            LOG(INFO, RPC_METADATA_TAG) << "going to parent " << parentofmynode << std::endl;
            lstat_response = std::move(rpc_lstat.on(get_engine().lookup(parentofmynode))(false, path_string));
            LOG(INFO, RPC_METADATA_TAG) << "returned from parent " << parentofmynode << std::endl;
        } else { // NOTE: request already made wait for cache to be filled
            LOG(INFO, RPC_METADATA_TAG) << " not first to request path_string, waiting on cache entry..." << std::endl;
            int status = CacheTables::md_path_status_cache_table.wait_on_cache_status(path_string);

            if(status != 0) {
                lstat_response = std::make_pair(status, std::vector<std::byte>(0));
            } else {
                assert(CacheTables::md_cache_table.get(path_string).has_value());
                lstat_response = std::make_pair(status, CacheTables::md_cache_table.get(path_string).value()->get_st_vec_cpy());
            }
        }
    }

    if(!cached && lstat_response.first == Constants::fs_operation_success) {
        LOG(INFO, RPC_METADATA_TAG) << "caching intermediate rpc_lstat_response" << std::endl;
        const auto lstat_response_copy = new CuStat{lstat_response.second};
        CacheTables::md_cache_table.put_force(path_string, std::move(*lstat_response_copy));
    } else if(cached) {
        LOG(INFO, RPC_METADATA_TAG) << "lstat response already cached" << std::endl;
    } else {
        LOG(INFO, RPC_METADATA_TAG) << "lstat response != 0, not caching intermediate rpc_lstat_response" << std::endl;
    }

    CacheTables::md_path_status_cache_table.update_cache_status(path_string, lstat_response.first);

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

    // NOTE: check if request can be resolved in local cache
    const auto entry_opt = CacheTables::data_cache_table.get(path_string);
    if(!dest && entry_opt.has_value()) {
        LOG(INFO, RPC_DATA_TAG) << "found in local cache" << std::endl;
        const auto bytes = entry_opt.value();
        read_response = std::make_pair(bytes->size(), std::move(*bytes));
        cached = true;
    } else if(Node::root->data == my_curr_node_addr) {
        LOG(INFO, RPC_DATA_TAG) << "requesting data from underlying filesystem" << std::endl;
        try {
            const std::vector<std::byte>& file_bytes = Util::read_ent_file(path_string, true);
            read_response = std::make_pair(file_bytes.size(), file_bytes);
        } catch(std::exception& e) {
            LOG(WARNING, RPC_DATA_TAG) << e.what() << std::endl;
            read_response = std::make_pair(-Constants::fs_operation_error, std::vector<std::byte>(0));
        }
    } else {
        // NOTE: were the first one to make rpc request
        if(!CacheTables::data_path_status_cache_table.check_and_put_force(path_string)) {
            LOG(INFO, RPC_DATA_TAG) << "first to request path_string" << std::endl;
            std::string parentofmynode;
            NodeTree::getParentfromtree(Node::root, my_curr_node_addr, parentofmynode);
            LOG(INFO, RPC_DATA_TAG) << "going to parent " << parentofmynode << std::endl;
            read_response = std::move(rpc_readfile.on(get_engine().lookup(parentofmynode))(false, path_string));
            LOG(INFO, RPC_DATA_TAG) << "returned from parent " << parentofmynode << std::endl;
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
        }
    }

    if(!cached && read_response.first >= 0) {
        LOG(INFO, RPC_READDIR_TAG) << "caching intermediate rpc_readfile_response" << std::endl;
        auto read_response_copy = read_response.second;
        CacheTables::data_cache_table.put_force(path_string, std::move(read_response_copy));
    } else if(cached) {
        LOG(INFO, RPC_DATA_TAG) << "readfile response already cached" << std::endl;
    } else {
        LOG(INFO, RPC_DATA_TAG) << "readfile response < 0, not caching intermediate rpc_readfile_response" << std::endl;
    }

    CacheTables::data_path_status_cache_table.update_cache_status(path_string, read_response.first);

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

    // NOTE: check if request can be resolved in local cache
    const auto entry_opt = CacheTables::tree_cache_table.get(path_string);
    bool cached = false;

    if(!dest && entry_opt.has_value()) {
        LOG(INFO, RPC_READDIR_TAG) << "found in local cache" << std::endl;
        const auto paths = entry_opt.value();
        readdir_response = std::make_pair(Constants::fs_operation_success, std::move(*paths));
        cached = true;
    } else if(Node::root->data == my_curr_node_addr) {
        LOG(INFO, RPC_READDIR_TAG) << "requesting data from underlying filesystem" << std::endl;
        auto entries = std::vector<std::string>{};
        DIR* dp = opendir(path_string.c_str());
        if(dp == nullptr) {
            LOG(WARNING) << "failed to passthrough readdir" << std::endl;
            readdir_response = std::make_pair(-errno, std::vector<std::string>{});
        } else {
            dirent* de;
            while((de = readdir(dp)) != nullptr) {
                entries.emplace_back(de->d_name);
            }
            closedir(dp);

            readdir_response = std::make_pair(Constants::fs_operation_success, entries);
        }
    } else {
        // NOTE: were the first one to make rpc request
        if(!CacheTables::tree_path_status_cache_table.check_and_put_force(path_string)) {
            LOG(INFO, RPC_READDIR_TAG) << "first to request path_string" << std::endl;
            std::string parentofmynode;
            NodeTree::getParentfromtree(Node::root, my_curr_node_addr, parentofmynode);
            LOG(INFO, RPC_READDIR_TAG) << "going to parent " << parentofmynode << std::endl;
            readdir_response = std::move(rpc_readdir.on(get_engine().lookup(parentofmynode))(false, path_string));
            LOG(INFO, RPC_READDIR_TAG) << "returned from parent " << parentofmynode << std::endl;
        } else { // NOTE: request already made wait for cache to be filled
            LOG(INFO, RPC_READDIR_TAG) << " not first to request path_string, waiting on cache entry..." << std::endl;
            const auto status = CacheTables::tree_path_status_cache_table.wait_on_cache_status(path_string);

            if(status != 0) {
                readdir_response = std::make_pair(status, std::vector<std::string>());
            } else {
                assert(CacheTables::tree_cache_table.get(path_string).has_value());
                readdir_response = std::make_pair(status, *CacheTables::tree_cache_table.get(path_string).value());
            }
        }
    }

    if(!cached && readdir_response.first == Constants::fs_operation_success) {
        LOG(INFO, RPC_READDIR_TAG) << "caching intermediate rpc_readfile_response" << std::endl;
        auto readdir_response_copy = readdir_response.second;
        CacheTables::tree_cache_table.put_force(path_string, std::move(readdir_response_copy));
    } else if(cached) {
        LOG(INFO, RPC_READDIR_TAG) << "readdir response already cached" << std::endl;
    } else {
        LOG(INFO, RPC_READDIR_TAG) << "readdir response != 0, not caching intermediate rpc_readfile_response" << std::endl;
    }

    CacheTables::tree_path_status_cache_table.update_cache_status(path_string, readdir_response.first);

    if(!dest) {
        TIME_RPC(req.respond(readdir_response));
    } else {
        // NOTE: final response so send status only
        LOG(INFO, RPC_READDIR_TAG) << "returning final response to client" << std::endl;
        TIME_RPC(req.respond(readdir_response.first));
    }
}
