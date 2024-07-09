#include "server_local_cache_provider.h"
#include <cassert>

std::vector<std::pair<std::string, std::string>> ServerLocalCacheProvider::global_peer_pairs;
std::mutex ServerLocalCacheProvider::mtx;
std::vector<std::string> ServerLocalCacheProvider::node_address_data;
std::vector<tl::endpoint> ServerLocalCacheProvider::m_peers;

tl::remote_procedure ServerLocalCacheProvider::rpc_lstat;
tl::remote_procedure ServerLocalCacheProvider::rpc_readfile;
tl::remote_procedure ServerLocalCacheProvider::rpc_readdir;

void ServerLocalCacheProvider::rpcLstat(const tl::request& req, const std::string& path_string) const {
    const auto& req_from_addr = static_cast<std::string>(req.get_endpoint());
    const auto& my_curr_node_addr = static_cast<std::string>(get_engine().self());

    LOG(INFO, RPC_METADATA_TAG) << "my address:" << my_curr_node_addr << std::endl;
    LOG(INFO, RPC_METADATA_TAG) << "req_coming_from_addr:" << req_from_addr << ", path_string: " << path_string << std::endl;

    const auto entry_opt = CacheTables::md_cache_table.get(path_string);

    // NOTE: check if it is in local cache
    if(entry_opt.has_value()) {
        LOG(INFO, RPC_METADATA_TAG) << "found in local cache" << std::endl;
        TIME_RPC(req.respond(static_cast<lstat_return_type>(std::make_pair(0, entry_opt.value()->get_vec()))));
    }
    // NOTE: check if we are root
    else if(Node::root->data == my_curr_node_addr) {
        LOG(INFO, RPC_METADATA_TAG) << "requesting data from underlying filesystem" << std::endl;
        CuStat cu_stat;

        // NOTE: we are root and operation succeeded respond to requested
        if(lstat(path_string.c_str(), cu_stat.get_st()) == -1) {
            const lstat_return_type& lstat_response = static_cast<lstat_return_type>(std::make_pair(-errno, std::vector<std::byte>(0)));
            TIME_RPC(req.respond(lstat_response));
            CacheTables::md_cache_table.put_force(path_string, std::move(cu_stat));
        } else {
            TIME_RPC(req.respond(static_cast<lstat_return_type>(std::make_pair(0, cu_stat.get_vec()))));
        }
    // NOTE: we are not root, and it is not in local cache so request data from parent only if we are not the root node
    } else {
        const auto& parent = NodeTree::get_parent_from_tree(Node::root, my_curr_node_addr);
        LOG(INFO, RPC_METADATA_TAG) << "parent node: " << parent << std::endl;
        const lstat_return_type& lstat_response = rpc_lstat.on(get_engine().lookup(parent))(path_string);
        TIME_RPC(req.respond(static_cast<lstat_return_type>(lstat_response)));
        CacheTables::md_cache_table.put_force(path_string, CuStat{lstat_response.second});
    }
}

void ServerLocalCacheProvider::rpcRead(const tl::request& req, const std::string& path_string) const {
    const auto& req_from_addr = static_cast<std::string>(req.get_endpoint());
    const auto& my_curr_node_addr = static_cast<std::string>(get_engine().self());

    LOG(INFO, RPC_DATA_TAG) << "my address:" << my_curr_node_addr << std::endl;
    LOG(INFO, RPC_DATA_TAG) << "req_coming_from_addr: " << req_from_addr << ", path_string: " << path_string << std::endl;

    const auto entry_opt = CacheTables::data_cache_table.get(path_string);
    // NOTE: check if it is in local cache
    if(entry_opt.has_value()) {
        LOG(INFO, RPC_DATA_TAG) << "found in local cache" << std::endl;
        const auto bytes = entry_opt.value();
        TIME_RPC(req.respond(static_cast<read_return_type>(std::make_pair(bytes->size(), *bytes))));
    }
    // NOTE: check if we are root
    else if(Node::root->data == my_curr_node_addr) {
        LOG(INFO, RPC_DATA_TAG) << "requesting data from underlying filesystem" << std::endl;
        try {
            std::vector<std::byte> file_bytes = Util::read_ent_file(path_string, true);
            TIME_RPC(req.respond(static_cast<read_return_type>(std::make_pair(file_bytes.size(), file_bytes))));
            CacheTables::data_cache_table.put_force(path_string, std::move(file_bytes));
        } catch(std::exception& e) {
            LOG(WARNING, RPC_DATA_TAG) << e.what() << std::endl;
            TIME_RPC(req.respond(static_cast<read_return_type>(std::make_pair(-Constants::fs_operation_error, std::vector<std::byte>(0)))));
        }
    // NOTE: we are not root, and it is not in local cache so request data from parent only if we are not the root node
    } else {
        const auto& parent = NodeTree::get_parent_from_tree(Node::root, my_curr_node_addr);
        read_return_type read_response = rpc_readfile.on(get_engine().lookup(parent))(path_string);
        TIME_RPC(req.respond(static_cast<read_return_type>(read_response)));
        CacheTables::data_cache_table.put_force(path_string, std::move(read_response.second));
    }
}

void ServerLocalCacheProvider::rpcReaddir(const tl::request& req, const std::string& path_string) const {
    const auto& req_from_addr = static_cast<std::string>(req.get_endpoint());
    const auto& my_curr_node_addr = static_cast<std::string>(get_engine().self());

    LOG(INFO, RPC_READDIR_TAG) << "my address:" << my_curr_node_addr << std::endl;
    LOG(INFO, RPC_READDIR_TAG) << "req_coming_from_addr: " << req_from_addr << ", path_string: " << path_string << std::endl;

    // NOTE: check if request can be resolved in local cache
    const auto entry_opt = CacheTables::tree_cache_table.get(path_string);
    // NOTE: check if it is in local cache
    if(entry_opt.has_value()) {
        LOG(INFO, RPC_READDIR_TAG) << "found in local cache" << std::endl;
        TIME_RPC(req.respond(static_cast<readdir_return_type >(std::make_pair(Constants::fs_operation_success, *entry_opt.value()))));
    // NOTE: check if we are root
    } else if(Node::root->data == my_curr_node_addr) {
        LOG(INFO, RPC_READDIR_TAG) << "requesting data from underlying filesystem" << std::endl;
        auto entries = std::vector<std::string>{};
        DIR* dp = opendir(path_string.c_str());
        if(dp == nullptr) {
            LOG(WARNING) << "failed to passthrough readdir" << std::endl;
            TIME_RPC(req.respond(static_cast<readdir_return_type >(std::make_pair(-errno, std::vector<std::string>{}))));
        } else {
            dirent* de;
            while((de = readdir(dp)) != nullptr) {
                entries.emplace_back(de->d_name);
            }
            closedir(dp);

            TIME_RPC(req.respond(static_cast<readdir_return_type >(std::make_pair(Constants::fs_operation_success, entries))));
            CacheTables::tree_cache_table.put_force(path_string, std::move(entries));
        }
    // NOTE: we are not root, and it is not in local cache so request data from parent only if we are not the root node
    } else {
        const auto& parent = NodeTree::get_parent_from_tree(Node::root, my_curr_node_addr);
        readdir_return_type readdir_response = rpc_readdir.on(get_engine().lookup(parent))(path_string);
        TIME_RPC(req.respond(static_cast<readdir_return_type>(readdir_response)));
        CacheTables::tree_cache_table.put_force(path_string, std::move(readdir_response.second));
    }
}
