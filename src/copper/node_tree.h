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

    Node(std::string data, Node* my_parent, int level, int child_id) {
        this->data = data;
        this->my_parent = my_parent;
        this->level = level;
        this->child_id = child_id;
    }

    void addChild(Node* child) {
        children.push_back(child);
    }

    std::vector<Node*> getChildren() {
        return children;
    }
};

class NodeTree {
    public:
    static void print_tree(Node* node);
    static void pretty_print_tree(Node* root, int depth, int dep_counter = 0);
    static void get_parent_from_tree(Node* CopyofTree, const std::string& my_curr_node_addr, std::string& parentofmynode);
    static int depth(Node* root);
    static Node* build_my_tree(Node* root, std::vector<std::string> node_address_data);
    static void push_back_address(const std::string& hostname, const std::string& my_cxi_server_ip_hex_str);
    static void get_hsn0_cxi_addr();
    static void parse_nodelist_from_cxi_address_book();
    static void NodeTree::generate_nodelist_from_nodefile(const std::string& filename);
};

#endif // NODE_TREE_H
