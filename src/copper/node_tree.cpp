#include "node_tree.h"

#include "server_local_cache_provider.h"

Node* Node::root = nullptr;

void NodeTree::print_tree(Node* node) {
    LOG(INFO) << "level: " << node->level << ", child id at this level: " << node->child_id << ", data: " << node->data
              << std::endl;
    for(Node* child : node->getChildren()) {
        print_tree(child);
    }
}

void NodeTree::pretty_print_tree(Node* root, int depth, int dep_counter) {
    if(root == nullptr) {
        return;
    }

    for(int i = 0; i < depth; i++) {
        LOG(INFO) << "    ";
    }

    LOG(INFO) << "(depth " << dep_counter << ") " << root->data << std::endl;

    for(Node* child : root->children) {
        pretty_print_tree(child, depth + 1, dep_counter + 1);
    }
}

int NodeTree::depth(Node* root) {
    if(root == nullptr) {
        return 0;
    }

    int maxDepth = 0;
    for(Node* child : root->children) {
        maxDepth = std::max(maxDepth, depth(child));
    }

    return maxDepth + 1;
}

Node* NodeTree::build_my_tree(Node* root, std::vector<std::string> node_address_data) {

    int max_children_per_parent = 1;

    // Based on a simple formula for sum of geometric series a(1-r^n)/(1-r) where n = 4 = max level5 -1, a=r=num_children

    if(node_address_data.size() <= 31)
        max_children_per_parent = 2;
    else if(node_address_data.size() <= 121)
        max_children_per_parent = 3;
    else if(node_address_data.size() <= 341)
        max_children_per_parent = 4;
    else if(node_address_data.size() <= 781)
        max_children_per_parent = 5;
    else if(node_address_data.size() <= 1555)
        max_children_per_parent = 6;
    else
        max_children_per_parent = 7;

    // LOG(INFO) <<  "max_children_per_parent : " << max_children_per_parent << std::endl;

    int current_children_per_parent = 0;
    int my_current_level = 0;
    int l1_child_counter = 0, l2_child_counter = 0, l3_child_counter = 0, l4_child_counter = 0, l5_child_counter = 0;
    int i_id_l2_parent = 0;                                                             // For level 2 children
    int i_id_l3_parent = 0, j_id_l3_parent = 0;                                         // For level 3 children
    int i_id_l4_parent = 0, j_id_l4_parent = 0, k_id_l4_parent = 0;                     // For level 4 children
    int i_id_l5_parent = 0, j_id_l5_parent = 0, k_id_l5_parent = 0, l_id_l5_parent = 0; // For level 5 children

    float my_current_level_before_trim;

    for(int i_node_counter = 0; i_node_counter < node_address_data.size(); i_node_counter++) {
        if(i_node_counter == 0) {
            root = new Node(node_address_data[i_node_counter], nullptr, 0, 1); // At level 0
            // LOG(INFO) << "node counter : " <<i_node_counter << " node ID: " << node_address_data[i_node_counter] << " level : " << my_current_level << std::endl;

        } else {
            // To redirect a node to a level
            // when max_children_per_parent = 2; floor (log (i) / log(2))
            // when max_children_per_parent = 3; floor (log (2 *i) / log(3))
            // when max_children_per_parent = 4; floor (log (3 *i) / log(4))

            my_current_level_before_trim = log((max_children_per_parent - 1) * i_node_counter + 1) / log(max_children_per_parent);
            my_current_level = floor(my_current_level_before_trim); // Dont touch this - log2(8) is not exactly 3 but only closer to 3

            // LOG(INFO) << "node counter : " <<i_node_counter << " node ID: " << node_address_data[i_node_counter] << " level : " << my_current_level << std::endl;


            if(my_current_level == 1) {
                l1_child_counter = l1_child_counter + 1;
                root->children.push_back(new Node(node_address_data[i_node_counter], root, my_current_level, l1_child_counter));

                if(l1_child_counter == max_children_per_parent) {
                    l1_child_counter = 0;
                }
            }

            if(my_current_level == 2) {
                l2_child_counter = l2_child_counter + 1;
                root->children[i_id_l2_parent]->children.push_back(new Node({node_address_data[i_node_counter]},
                root->children[i_id_l2_parent], my_current_level, l2_child_counter));

                if(l2_child_counter == max_children_per_parent) {
                    l2_child_counter = 0;
                    i_id_l2_parent = i_id_l2_parent + 1;
                }
            }

            if(my_current_level == 3) {
                l3_child_counter = l3_child_counter + 1;
                root->children[i_id_l3_parent]->children[j_id_l3_parent]->children.push_back(
                new Node({node_address_data[i_node_counter]}, root->children[i_id_l3_parent]->children[j_id_l3_parent],
                my_current_level, l3_child_counter));

                if(l3_child_counter == max_children_per_parent) {
                    l3_child_counter = 0;
                    j_id_l3_parent = j_id_l3_parent + 1;

                    if(j_id_l3_parent == max_children_per_parent) {
                        j_id_l3_parent = 0;
                        i_id_l3_parent = i_id_l3_parent + 1;
                    }
                }
            }

            if(my_current_level == 4) {
                l4_child_counter = l4_child_counter + 1;
                root->children[i_id_l4_parent]->children[j_id_l4_parent]->children[k_id_l4_parent]->children.push_back(
                new Node({node_address_data[i_node_counter]},
                root->children[i_id_l4_parent]->children[j_id_l4_parent]->children[k_id_l4_parent], my_current_level,
                l4_child_counter));

                if(l4_child_counter == max_children_per_parent) {
                    l4_child_counter = 0;
                    k_id_l4_parent = k_id_l4_parent + 1;

                    if(k_id_l4_parent == max_children_per_parent) {
                        k_id_l4_parent = 0;
                        j_id_l4_parent = j_id_l4_parent + 1;

                        if(j_id_l4_parent == max_children_per_parent) {
                            j_id_l4_parent = 0;
                            i_id_l4_parent = i_id_l4_parent + 1;
                        }
                    }
                }
            }

            if(my_current_level == 5) {
                l5_child_counter = l5_child_counter + 1;
                root->children[i_id_l5_parent]
                ->children[j_id_l5_parent]
                ->children[k_id_l5_parent]
                ->children[l_id_l5_parent]
                ->children.push_back(new Node({node_address_data[i_node_counter]},
                root->children[i_id_l5_parent]->children[j_id_l5_parent]->children[k_id_l5_parent]->children[l_id_l5_parent],
                my_current_level, l5_child_counter));

                if(l5_child_counter == max_children_per_parent) {
                    l5_child_counter = 0;
                    l_id_l5_parent = l_id_l5_parent + 1;

                    if(l_id_l5_parent == max_children_per_parent) {
                        l_id_l5_parent = 0;
                        k_id_l5_parent = k_id_l5_parent + 1;

                        if(k_id_l5_parent == max_children_per_parent) {
                            k_id_l5_parent = 0;
                            j_id_l5_parent = j_id_l5_parent + 1;

                            if(j_id_l5_parent == max_children_per_parent) {
                                j_id_l5_parent = 0;
                                i_id_l5_parent = i_id_l5_parent + 1;
                            }
                        }
                    }
                }
            }
        }
    }
    return root;
}


void NodeTree::push_back_address(const std::string& hostname, const std::string& my_cxi_server_ip_hex_str) {
    // NOTE: 0644 - read and write permissions for the owner and read-only permissions for others
    const int fd = open(Constants::copper_address_book_path.c_str(), O_WRONLY | O_APPEND | O_CREAT, 0644);
    if(fd == -1) {
        throw std::runtime_error("failed to open file for writing");
    }

    if(flock(fd, LOCK_EX) == -1) {
        close(fd);
        throw std::runtime_error("failed to lock file");
    }

    const std::string data = hostname + " " + my_cxi_server_ip_hex_str + "\n";
    if(write(fd, data.c_str(), data.size()) == -1) {
        flock(fd, LOCK_UN); // Unlock the file before closing
        close(fd);          // Close the file descriptor
        throw std::runtime_error("failed to write to file");
    }

    flock(fd, LOCK_UN);
    close(fd);
}

void NodeTree::get_hsn0_cxi_addr() {
    std::string my_hsn0_mac_id;
    std::ifstream inFile("/sys/class/net/hsn0/address");
    if(!inFile.is_open()) {
        LOG(FATAL) << "error opening address file" << std::endl;
        throw std::runtime_error("error opening address file");
    }
    inFile >> my_hsn0_mac_id;
    inFile.close();

    std::erase(my_hsn0_mac_id, ':');
    std::string my_hsn0_nic_id = my_hsn0_mac_id.substr(my_hsn0_mac_id.length() - 5);

    std::stringstream my_hsn0_nic_id_bin_ss;
    my_hsn0_nic_id_bin_ss << std::hex << my_hsn0_nic_id;
    unsigned my_hsn0_nic_id_bin_us;
    my_hsn0_nic_id_bin_ss >> my_hsn0_nic_id_bin_us;
    std::bitset<20> my_hsn0_nic_id_bin(my_hsn0_nic_id_bin_us);

    // valid bits(3) + NIC (20) + PID (9)
    std::string my_cxi_server_ip_binary_str = "001" + my_hsn0_nic_id_bin.to_string() + "000000000";

    std::bitset<32> my_cxi_server_ip_binary(my_cxi_server_ip_binary_str);

    std::stringstream my_cxi_server_ip_hex_ss;
    my_cxi_server_ip_hex_ss << std::hex << my_cxi_server_ip_binary.to_ulong();
    std::string my_cxi_server_ip_hex_str = "ofi+cxi://0x" + my_cxi_server_ip_hex_ss.str();

    char char_hostname[1024];
    gethostname(char_hostname, sizeof(char_hostname));
    std::string hostname(char_hostname);

    // replace mutex with oneapi/tbb/concurrent_vector.h tbb::concurrent_vector is not working currently with oneapi
    push_back_address(hostname, my_cxi_server_ip_hex_str);
}

void NodeTree::parse_nodelist_from_cxi_address_book() {
    sleep(20); //  barrier issue: The first process needs to wait until all the remaining processes have written to the address book.

    std::ifstream inFile(Constants::copper_address_book_path, std::ios::in);

    LOG(INFO) << "opening file" << std::endl;
    if(!inFile.is_open()) {
        LOG(FATAL) << "error opening copper address file at path: " << Constants::copper_address_book_path << std::endl;
        throw std::runtime_error("error opening copper address file");
    }

    std::string line;
    while(getline(inFile, line)) {
        LOG(INFO) << getpid() << ":" << line << std::endl;
        size_t pos = line.find(" ");
        std::string first_part_hostname = line.substr(0, pos);
        std::string second_part_cxi = line.substr(pos + 1);
        LOG(INFO) << first_part_hostname << ":" << second_part_cxi << std::endl;
        ServerLocalCacheProvider::global_peer_pairs.emplace_back(first_part_hostname, second_part_cxi);
        ServerLocalCacheProvider::node_address_data.emplace_back(second_part_cxi);
    }

    inFile.close();
}


void NodeTree::get_parent_from_tree(Node* CopyofTree, const std::string& my_curr_node_addr, std::string& parentofmynode) {
    if(my_curr_node_addr == CopyofTree->data) {
        parentofmynode = CopyofTree->my_parent->data;
    }
    for(Node* child : CopyofTree->getChildren()) {
        get_parent_from_tree(child, my_curr_node_addr, parentofmynode);
    }
}
