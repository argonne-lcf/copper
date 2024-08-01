#ifndef NODE_TREE_H
#define NODE_TREE_H

#include <fcntl.h>
#include <mutex>
#include <string>
#include <sys/file.h>
#include <vector>

#include "../aixlog.h"

class Node {
    public:
    static Node* root;
    std::string data;
    Node* my_parent;
    std::vector<Node*> children;
    int level;
    int child_id;

    Node(std::string data, Node* parent, int level, int child_id) : data{std::move(data)}, my_parent{parent}, level{level}, child_id{child_id} {
    }

    void add_child(Node* child) {
        children.push_back(child);
    }

    std::vector<Node*> get_children() const {
        return children;
    }
};

class NodeTree {
    public:
    static void print_tree(Node* node);
    static void pretty_print_tree(Node* root, int depth, int dep_counter = 0);
    static void get_parent_from_tree(Node* copy_of_tree, const std::string& my_curr_node_addr, std::string& parent);
    static int depth(Node* root);
    static Node* build_my_tree(Node* root, std::vector<std::string> node_address_data);
    static void push_back_address(const std::string& hostname, const std::string& my_cxi_server_ip_hex_str);
    static void get_hsn0_cxi_addr();
    static void parse_nodelist_from_address_book();
    static void generate_nodelist_from_nodefile(const std::string& filename);
};

#endif // NODE_TREE_H
