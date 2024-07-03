#include "server_local_cache_provider.h"

#include <thallium/serialization/stl/pair.hpp>
#include <thallium/serialization/stl/string.hpp>
#include <thallium/serialization/stl/vector.hpp>

std::vector<std::pair<std::string, std::string>> ServerLocalCacheProvider::global_peer_pairs;
std::mutex ServerLocalCacheProvider::mtx;
std::vector<std::string> ServerLocalCacheProvider::node_address_data;
std::vector<tl::endpoint> ServerLocalCacheProvider::m_peers;
std::string ServerLocalCacheProvider::my_hostname;

void ServerLocalCacheProvider::rpcLstat(const tl::request& req, const std::string& path_string) {
    std::string req_from_addr = static_cast<std::string>(req.get_endpoint());
    LOG(INFO, RPC_METADATA_TAG)
    << "req_coming_from_addr:" << req_from_addr << ", requested metadata for file: " << path_string << std::endl;

    // NOTE: check if request can be resolved in local cache
    const auto entry_opt = CacheTables::md_cache_table.get(path_string);
    if(entry_opt.has_value()) {
        LOG(INFO, RPC_METADATA_TAG) << "found in local cache" << std::endl;
        TIME_RPC(req.respond(lstat_return_type(Constants::fs_operation_success, entry_opt.value()->get_st_vec_cpy())));
        return;
    } else {
        LOG(INFO, RPC_METADATA_TAG) << "not found in local cache" << std::endl;
    }

    std::string my_curr_node_addr = static_cast<std::string>(get_engine().self());

    if(Node::root->data == my_curr_node_addr) {
        CuStat cu_stat;

        if(lstat(path_string.c_str(), cu_stat.get_st()) == -1) {
            TIME_RPC(req.respond(lstat_return_type(-errno, std::vector<std::byte>(0))));
        } else {
            TIME_RPC(req.respond(lstat_return_type(0, cu_stat.get_move_st_vec())));
        }
    } else {
        std::string parentofmynode;
        getParentfromtree(Node::root, my_curr_node_addr, parentofmynode);

        LOG(INFO, RPC_METADATA_TAG) << "going to parent " << parentofmynode << std::endl;
        lstat_return_type rpc_lstat_response = std::move(rpc_lstat.on(get_engine().lookup(parentofmynode))(path_string));
        LOG(INFO, RPC_METADATA_TAG) << "returned from parent " << parentofmynode << std::endl;

        if(rpc_lstat_response.first == 0) {
            LOG(INFO, RPC_METADATA_TAG) << "caching intermediate rpc_lstat_response" << std::endl;

            // NOTE: copies RPC response and moves into cache
            auto new_cu_stat = new CuStat{rpc_lstat_response.second};
            CacheTables::md_cache_table.put_force(path_string, std::move(*new_cu_stat));
        } else {
            LOG(INFO, RPC_METADATA_TAG) << "lstat response != 0, not caching intermediate lstat_response" << std::endl;
        }

        LOG(INFO, RPC_METADATA_TAG) << "hop trip my_curr_node_addr " << my_curr_node_addr << std::endl;
        TIME_RPC(req.respond(rpc_lstat_response));
    }
}

void ServerLocalCacheProvider::rpcRead(const tl::request& req, const std::string& path_string) {
     std::string req_from_addr = static_cast<std::string>(req.get_endpoint());
     LOG(INFO, RPC_TAG) << "req_coming_from_addr: " << req_from_addr << ", requested data for file: " << path_string << std::endl;

     // NOTE: check if request can be resolved in local cache
     const auto entry_opt = CacheTables::data_cache_table.get(path_string);
     if(entry_opt.has_value()) {
         LOG(INFO, RPC_DATA_TAG) << "found in local cache" << std::endl;
         auto bytes = entry_opt.value();
         LOG(INFO, RPC_DATA_TAG) << "bytes size: " << bytes->size() << std::endl;
         TIME_RPC(req.respond(read_return_type(bytes->size(), std::vector<std::byte>(*bytes))));
         return;
     } else {
         LOG(INFO, RPC_DATA_TAG) << "not found in local cache" << std::endl;
     }

     std::string my_curr_node_addr = static_cast<std::string>(get_engine().self());

     if(Node::root->data == my_curr_node_addr) {
         try {
             const std::vector<std::byte>& file_bytes = Util::read_ent_file(path_string, true);
             LOG(INFO, RPC_DATA_TAG) << "bytes size: " << file_bytes.size() << std::endl;
             TIME_RPC(req.respond(read_return_type(0, file_bytes)));
             return;
         } catch(std::exception& e) {
             LOG(WARNING, RPC_DATA_TAG) << e.what() << std::endl;
             LOG(INFO, RPC_DATA_TAG) << "bytes size: " << 0 << std::endl;
             TIME_RPC(req.respond(read_return_type(-1, std::vector<std::byte>(0))));
         }
     } else {
         std::string parentofmynode;
         getParentfromtree(Node::root, my_curr_node_addr, parentofmynode);
         LOG(INFO, RPC_DATA_TAG) << "going to parent " << parentofmynode << std::endl;
         read_return_type rpc_readfile_response = std::move(rpc_readfile.on(get_engine().lookup(parentofmynode))(path_string));
         LOG(INFO, RPC_DATA_TAG) << "returned from parent " << parentofmynode << std::endl;

         if(rpc_readfile_response.first >= 0) {
             LOG(INFO, RPC_DATA_TAG) << "caching intermediate rpc_readfile_response" << std::endl;

             // NOTE: copies RPC response and moves into cache
             auto rpc_readfile_response_byte_copy = rpc_readfile_response.second;
             CacheTables::data_cache_table.put_force(path_string, std::move(rpc_readfile_response_byte_copy));
         } else {
             LOG(INFO, RPC_DATA_TAG) << "readfile response < 0, not caching intermediate rpc_readfile_response" << std::endl;
         }

         LOG(INFO, RPC_DATA_TAG) << "hop trip my_curr_node_addr: " << my_curr_node_addr << std::endl;
         LOG(INFO, RPC_DATA_TAG) << "bytes size: " << rpc_readfile_response.second.size() << std::endl;
         TIME_RPC(req.respond(rpc_readfile_response));
     }
 }

 void ServerLocalCacheProvider::rpcReaddir(const tl::request& req, const std::string& path_string) {
     std::string req_from_addr = static_cast<std::string>(req.get_endpoint());
     LOG(INFO, RPC_TAG) << "req_coming_from_addr: " << req_from_addr << ", requested readdir for file: " << path_string << std::endl;

     // NOTE: check if request can be resolved in local cache
     const auto entry_opt = CacheTables::tree_cache_table.get(path_string);
     if(entry_opt.has_value()) {
         LOG(INFO, RPC_READDIR_TAG) << "found in local cache" << std::endl;
         auto paths = entry_opt.value();
         LOG(INFO, RPC_READDIR_TAG) << "num paths: " << paths->size() << std::endl;
         TIME_RPC(req.respond(readdir_return_type(Constants::fs_operation_success, std::vector<std::string>(*paths))));
         return;
     } else {
         LOG(INFO, RPC_READDIR_TAG) << "not found in local cache" << std::endl;
     }

     std::string my_curr_node_addr = static_cast<std::string>(get_engine().self());

     if(Node::root->data == my_curr_node_addr) {
         auto entries = std::vector<std::string>{};
         DIR* dp = opendir(path_string.c_str());
         if(dp == nullptr) {
             LOG(WARNING) << "failed to passthrough readdir" << std::endl;
             TIME_RPC(req.respond(readdir_return_type(-errno, std::vector<std::string>{})));
             return;
         }

         dirent* de;
         while((de = readdir(dp)) != nullptr) {
             LOG(TRACE) << "dir entry: " << de->d_name << std::endl;
             entries.emplace_back(de->d_name);
         }
         closedir(dp);

         TIME_RPC(req.respond(readdir_return_type(Constants::fs_operation_success, entries)));
     } else {
         std::string parentofmynode;
         getParentfromtree(Node::root, my_curr_node_addr, parentofmynode);
         LOG(INFO, RPC_READDIR_TAG) << "going to parent " << parentofmynode << std::endl;
         readdir_return_type rpc_readdir_response = std::move(rpc_readdir.on(get_engine().lookup(parentofmynode))(path_string));
         LOG(INFO, RPC_READDIR_TAG) << "returned from parent " << parentofmynode << std::endl;

         if(rpc_readdir_response.first == Constants::fs_operation_success) {
             LOG(INFO, RPC_READDIR_TAG) << "caching intermediate rpc_readfile_response" << std::endl;

             // NOTE: copies RPC response and moves into cache
             auto rpc_readdir_response_copy = rpc_readdir_response.second;
             CacheTables::tree_cache_table.put_force(path_string, std::move(rpc_readdir_response_copy));
         } else {
             LOG(INFO, RPC_READDIR_TAG) << "readdir response != 0, not caching intermediate rpc_readfile_response" << std::endl;
         }

         LOG(INFO, RPC_READDIR_TAG) << "hop trip my_curr_node_addr: " << my_curr_node_addr << std::endl;
         LOG(INFO, RPC_READDIR_TAG) << "num paths: " << rpc_readdir_response.second.size() << std::endl;
         TIME_RPC(req.respond(rpc_readdir_response));
     }
 }

void ServerLocalCacheProvider::getParentfromtree(Node* CopyofTree, std::string my_curr_node_addr, std::string& parentofmynode) {
    if(my_curr_node_addr == CopyofTree->data) {
        parentofmynode = CopyofTree->my_parent->data;
    }
    for(Node* child : CopyofTree->getChildren()) {
        getParentfromtree(child, my_curr_node_addr, parentofmynode);
    }
}
