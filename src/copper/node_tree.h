#ifndef NODE_TREE_H
#define NODE_TREE_H

#include <fcntl.h>
#include <mutex>
#include <string>
#include <sys/file.h>
#include <vector>
#include <optional>

#include "../aixlog.h"

class Node {
    public:
    static inline Node* root{nullptr};
    const Node* parent{nullptr};
    std::vector<Node*> children;

    const int level;
    const std::string addr;
    static inline std::optional<std::string> parent_addr{std::nullopt};
    const int child_id;

    Node(std::string addr, Node* parent, const int level, const int child_id) : addr{std::move(addr)}, parent{parent}, level{level}, child_id{child_id} {
    }

    [[nodiscard]] std::vector<Node*> get_children() const {
        return children;
    }
};

class NodeTree {
    public:
    static std::vector<std::string> get_my_tree_segment(std::vector<std::string>& addresses, std::string& my_addr, int num_segments);
    static void print_tree(const Node* node);
    static void pretty_print_tree(const Node* root, int depth, int dep_counter = 0);
    static void get_parent_from_tree(const Node* copy_of_tree, const std::string& my_curr_node_addr, std::string& parent);
    static int depth(const Node* root);
    static Node* build_my_tree(Node* root, std::vector<std::string> node_address_data);
    static void push_back_address(const std::string& hostname, const std::string& my_cxi_server_ip_hex_str);
    static void get_hsn0_cxi_addr();
    static void parse_nodelist_from_address_book();
    static void generate_nodelist_from_nodefile(const std::string& filename);
};

#endif // NODE_TREE_H
