#include "server_local_cache_provider.h"

#include <thallium/serialization/stl/pair.hpp>
#include <thallium/serialization/stl/string.hpp>
#include <thallium/serialization/stl/vector.hpp>

std::vector<std::pair<std::string, std::string>> ServerLocalCacheProvider::global_peer_pairs;
std::mutex ServerLocalCacheProvider::mtx;
std::vector<std::string> ServerLocalCacheProvider::node_address_data;
std::string ServerLocalCacheProvider::copper_address_book_name = "./copper_address_book";
std::vector<tl::endpoint> ServerLocalCacheProvider::m_peers;
std::string ServerLocalCacheProvider::my_hostname;

void ServerLocalCacheProvider::rpcLstat(const tl::request& req, const std::string& path_string) {
    std::string req_from_addr = static_cast<std::string>(req.get_endpoint());
    std::cout << "req_coming_from_addr " << req_from_addr << " requested metadata for file : " << path_string << std::endl;

    std::string my_curr_node_addr = static_cast<std::string>(get_engine().self());

    if(Node::root->data == my_curr_node_addr) {
        std::chrono::time_point<std::chrono::system_clock> start1, end1;
        start1 = std::chrono::system_clock::now();

        struct stat st;
        if(lstat(path_string.c_str(), &st) == -1) {
            req.respond(lstat_return_type(-errno, std::vector<std::byte>(0)));
            return;
        }

        std::vector<std::byte> stat_bytes(
        reinterpret_cast<std::byte*>(&st), reinterpret_cast<std::byte*>(&st) + sizeof(struct stat));
        req.respond(lstat_return_type(0, stat_bytes));

        end1 = std::chrono::system_clock::now();
        std::chrono::duration<double> elapsed_seconds1 = end1 - start1;
        std::cout << "for requester " << req_from_addr << " root buffer creation time " << elapsed_seconds1.count()
                  << " s " << std::endl;
    } else {
        Node* CopyofTree = Node::root;
        std::string parentofmynode = "";
        getParentfromtree(CopyofTree, my_curr_node_addr, parentofmynode);
        std::cout << "going to parent " << parentofmynode << std::endl;
        lstat_return_type lstat_response = rpc_lstat.on(get_engine().lookup(parentofmynode))(path_string);
        std::cout << "hop trip my_curr_node_addr " << my_curr_node_addr << std::endl;
        req.respond(lstat_response);
    }
}

void ServerLocalCacheProvider::rpcRead(const tl::request& req, const std::string& path_string) {
    std::string req_from_addr = static_cast<std::string>(req.get_endpoint());
    std::cout << "req_coming_from_addr " << req_from_addr << " requested data for file : " << path_string << std::endl;

    std::string my_curr_node_addr = static_cast<std::string>(get_engine().self());

    if(Node::root->data == my_curr_node_addr) {
        std::chrono::time_point<std::chrono::system_clock> start1, end1;
        start1 = std::chrono::system_clock::now();

        try {
            std::vector<std::byte> file_bytes = Util::read_ent_file(path_string, true);
            std::cout << "from rpcRead bytes size: " << file_bytes.size() << std::endl;
            req.respond(read_return_type(0, file_bytes));
            return;
        } catch(std::exception& e) {
            std::cout << e.what() << std::endl;
            req.respond(read_return_type(-1, std::vector<std::byte>(0)));
        }

        end1 = std::chrono::system_clock::now();
        std::chrono::duration<double> elapsed_seconds1 = end1 - start1;
        std::cout << "for requester " << req_from_addr << " root buffer creation time " << elapsed_seconds1.count()
                  << " s " << std::endl;
    } else {
        Node* CopyofTree = Node::root;
        std::string parentofmynode = "";
        getParentfromtree(CopyofTree, my_curr_node_addr, parentofmynode);
        std::cout << "going to parent " << parentofmynode << std::endl;
        read_return_type read_response = rpc_readfile.on(get_engine().lookup(parentofmynode))(path_string);
        std::cout << "hop trip my_curr_node_addr " << my_curr_node_addr << std::endl;
        req.respond(read_response);
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
