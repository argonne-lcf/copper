#ifndef NODE_TREE_H
#define NODE_TREE_H

#include <algorithm>
#include <array>
#include <bitset>
#include <chrono>
#include <climits>
#include <cmath>
#include <cstdlib>
#include <ctime>
#include <fstream>
#include <iostream>
#include <iterator>
#include <mutex>
#include <queue>
#include <sstream>
#include <string>
#include <sys/socket.h>
#include <sys/stat.h>
#include <thallium.hpp>
#include <thallium/serialization/stl/pair.hpp>
#include <thallium/serialization/stl/string.hpp>
#include <thallium/serialization/stl/vector.hpp>
#include <thread>
#include <unistd.h>
#include <vector>

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

    void getParent(Node* child) {
    }
    void getSibilings(Node* child) {
    }
    void getPathToRoot(Node* child) {
    }
    void getPathToLeaf(Node* child) {
    }
    void getBFS(Node* child) {
    }
    void getDFS(Node* child) {
    }
};

class NodeTree {
    public:
    static void printTree(Node* node);
    static void prettyPrintTree(Node* root, int depth, int dep_counter = 0);
    static int depth(Node* root);
    static Node* build_my_tree(Node* root, std::vector<std::string> node_address_data);
    static void push_back_with_mutex(std::string hostname, std::string my_cxi_server_ip_hex_str);
    static void get_hsn0_cxi_addr();
    static void parse_nodelist_from_cxi_address_book();
};

#endif // NODE_TREE_H
