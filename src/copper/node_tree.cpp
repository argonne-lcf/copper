#include "node_tree.h"

#include "server_local_cache_provider.h"

Node* Node::root = nullptr;

void NodeTree::printTree(Node* node) {
    std::cout << " Level " << node->level << " Child ID at this Level " << node->child_id << " Data " << node->data << std::endl;
    for(Node* child : node->getChildren()) {
        printTree(child);
    }
}

void NodeTree::prettyPrintTree(Node* root, int depth, int dep_counter) {
    if(root == nullptr) {
        return;
    }

    for(int i = 0; i < depth; i++) {
        std::cout << "    ";
    }

    std::cout << "(depth " << dep_counter << ") " << root->data << std::endl;

    for(Node* child : root->children) {
        prettyPrintTree(child, depth + 1, dep_counter + 1);
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

    // std::cout <<  "max_children_per_parent : " << max_children_per_parent << std::endl;

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
            // std::cout << "node counter : " <<i_node_counter << " node ID: " << node_address_data[i_node_counter] << " level : " << my_current_level << std::endl;

        } else {
            // To redirect a node to a level
            // when max_children_per_parent = 2; floor (log (i) / log(2))
            // when max_children_per_parent = 3; floor (log (2 *i) / log(3))
            // when max_children_per_parent = 4; floor (log (3 *i) / log(4))

            my_current_level_before_trim = log((max_children_per_parent - 1) * i_node_counter + 1) / log(max_children_per_parent);
            my_current_level = floor(my_current_level_before_trim); // Dont touch this - log2(8) is not exactly 3 but only closer to 3

            // std::cout << "node counter : " <<i_node_counter << " node ID: " << node_address_data[i_node_counter] << " level : " << my_current_level << std::endl;


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

void NodeTree::push_back_with_mutex(std::string hostname, std::string my_cxi_server_ip_hex_str) {

    std::lock_guard<std::mutex> lock(ServerLocalCacheProvider::mtx);
    std::ofstream myFile(ServerLocalCacheProvider::copper_address_book_name, std::ios_base::app);
    myFile << hostname << " " << my_cxi_server_ip_hex_str << std::endl;
    myFile.close();
}

void NodeTree::get_hsn0_cxi_addr() {
    std::string my_hsn0_mac_id;
    std::ifstream inFile("/sys/class/net/eth0/address");
    if(!inFile.is_open()) {
        std::cerr << "Error opening file" << std::endl;
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
    // std::cout << my_cxi_server_ip_binary_str << std::endl;

    std::bitset<32> my_cxi_server_ip_binary(my_cxi_server_ip_binary_str);

    std::stringstream my_cxi_server_ip_hex_ss;
    my_cxi_server_ip_hex_ss << std::hex << my_cxi_server_ip_binary.to_ulong();
    std::string my_cxi_server_ip_hex_str = "na+sm://0x" + my_cxi_server_ip_hex_ss.str();
    // std::cout << "my_cxi_server_ip_hex_str " << my_cxi_server_ip_hex_str << std::endl;

    char char_hostname[1024];
    gethostname(char_hostname, sizeof(char_hostname));
    std::string hostname(char_hostname);
    // std::cout << hostname << std::endl;

    // replace mutex with oneapi/tbb/concurrent_vector.h tbb::concurrent_vector is not working currently with oneapi
    push_back_with_mutex(hostname, my_cxi_server_ip_hex_str);
}

void NodeTree::parse_nodelist_from_cxi_address_book() {
    sleep(5); //  barrier issue: The first process needs to wait until all the remaining processes have written to the address book.

    std::ifstream inFile(ServerLocalCacheProvider::copper_address_book_name, std::ios::in);

    std::cout << "opening file" << std::endl;
    if(!inFile.is_open()) {
        std::cout << "Error opening file" << std::endl;
        return;
    }

    std::string line;

    while(getline(inFile, line)) {
        std::cout << getpid() << ":" << line << std::endl;
        size_t pos = line.find(" ");
        std::string first_part_hostname = line.substr(0, pos);
        std::string second_part_cxi = line.substr(pos + 1);
        std::cout << first_part_hostname << ":" << second_part_cxi << std::endl;
        ServerLocalCacheProvider::global_peer_pairs.push_back(make_pair(first_part_hostname, second_part_cxi));
        ServerLocalCacheProvider::node_address_data.push_back(second_part_cxi);
    }

    inFile.close();
}