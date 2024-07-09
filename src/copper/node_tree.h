#ifndef NODE_TREE_H
#define NODE_TREE_H


#include <fcntl.h>
#include <mutex>
#include <string>
#include <sys/file.h>
#include <utility>
#include <vector>

#include "../aixlog.h"

class Node {
    public:
    static Node* root;

    const std::string data;
    const Node* my_parent;
    std::vector<Node*> children;
    const int level;
    const int child_id;

    Node(std::string data, Node* my_parent, int level, int child_id) : data{std::move(data)}, my_parent{my_parent},
      level{level}, child_id{child_id} {
    }

    void add_child(Node* child) {
        children.push_back(child);
    }

    [[nodiscard]] std::vector<Node*> get_children() const {
        return children;
    }
};

class NodeTree {
    public:
    static void print_tree(Node* node);
    static void pretty_print_tree(Node* root, int depth, int dep_counter = 0);
    static std::string get_parent_from_tree(const Node* node, const std::string& my_curr_node_addr);
    static int depth(Node* root);
    static Node* build_my_tree(Node* root, std::vector<std::string> node_address_data);
    static void push_back_address(const std::string& hostname, const std::string& my_cxi_server_ip_hex_str);
    static void get_hsn0_cxi_addr();
    static void parse_nodelist_from_cxi_address_book();
};

#endif // NODE_TREE_H
