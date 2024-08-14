// module use /soft/preview-modulefiles/24.086.0
// module load frameworks/2024.04.15.002
// . /lus/gila/projects/CSC250STDM10_CNDA/kaushik/copper/git-spack/spack/share/spack/setup-env.sh 
// spack env activate kaushik_env_1 
// make file=copper 
// mpirun --env MARGO_ENABLE_MONITORING=1 --env MARGO_MONITORING_FILENAME_PREFIX="${PBS_JOBID}_${NNODES}_${NRANKS}_${RANKS_PER_NODE}_${BUF_SIZE}_" -np ${NRANKS} -ppn ${RANKS_PER_NODE} ./copper ${BUF_SIZE} 
// mpirun  -np 7 -ppn 1 ./copper 8 

#include <iostream>
#include <thallium.hpp>
#include <thallium/serialization/stl/string.hpp>
#include <array>
#include <vector>
#include <string>
#include <fstream>
#include <sys/socket.h>
#include <unistd.h> 
#include <cmath>
#include <mutex>
#include <sstream>
#include <bitset>
#include <algorithm>
#include <iterator>
#include <cstdlib>
#include <thread>
#include <queue>
#include <chrono>
#include <ctime>

namespace tl = thallium;

std::vector<std::pair<std::string, std::string>> global_peer_pairs;
std::mutex mtx;
std::vector<std::string> node_address_data;
std::string copper_address_book_name = "./copper_address_book.txt";

class Node 
{
    public:
        std::string data;
        Node* my_parent;
        std::vector<Node*> children;
        int level;
        int child_id;

    Node(std::string data, Node* my_parent, int level, int child_id) 
    {
        this->data = data;
        this->my_parent = my_parent;
        this->level = level;
        this->child_id = child_id;
    }

    void addChild(Node* child) 
    {
        children.push_back(child);
    }

    std::vector<Node*> getChildren() 
    {
        return children;
    }

    void getParent(Node* child) 
    {

    }

    void getSibilings(Node* child) 
    {

    }

    void getPathToRoot(Node* child) 
    {

    }

    void getPathToLeaf(Node* child) 
    {

    }

    void getBFS(Node* child) 
    {

    }

    void getDFS(Node* child) 
    {

    }
};

Node* root;
class ServerLocalCacheProvider : public tl::provider<ServerLocalCacheProvider> 
{
    std::vector<tl::endpoint> m_peers;
    tl::remote_procedure      m_get_file_data;
    public:
        ServerLocalCacheProvider(tl::engine serverEngine, const std::vector<std::string>& addresses) : 
        tl::provider<ServerLocalCacheProvider>{std::move(serverEngine), 0} , m_get_file_data{define("get_file_data", &ServerLocalCacheProvider::getFileData)}
        {
            get_engine().push_finalize_callback([this](){ delete this;});
            m_peers.reserve(addresses.size());

            for(auto& address : addresses) 
            {
                m_peers.push_back(get_engine().lookup(address));
                // std::cout << address << std::endl; 
            }
        }

        void getFileData(const tl::request& req, const std::string& filename, const int req_bytes) 
        {
            std::string req_from_addr = static_cast<std::string>(req.get_endpoint());
            std::cout << "req_coming_from_addr " << req_from_addr  << " requested data for file : " << filename << " requested bytes : " << req_bytes << std::endl;

            std::string my_curr_node_addr = static_cast<std::string>(get_engine().self());

            // if(root->data == static_cast<std::string>(req.get_endpoint()))
            if(root->data == my_curr_node_addr)
            {
                std::chrono::time_point<std::chrono::system_clock> start1, end1;
                start1 = std::chrono::system_clock::now();
                
                char* buffer = new char[req_bytes];
                for (int i = 0; i < req_bytes; i++) 
                {
                    buffer[i] = 'c';
                }
                std::string file_content =buffer;

                end1 = std::chrono::system_clock::now();           
                std::chrono::duration<double> elapsed_seconds1 = end1 - start1;
                std::cout << "For requester " << req_from_addr << " Root Buffer creation time " << elapsed_seconds1.count() << " s " << " Bytes " << req_bytes << std::endl ; 

                req.respond(file_content);
    
                // margo_instance_id mid;
                // mid  = get_engine().get_margo_instance();
                // char* state_file_name;
                // margo_state_dump(mid, "margo-example-server", 1, &state_file_name);
                // printf("# Runtime state dumped to %s\n", state_file_name);
                // free(state_file_name);
            }
            else
            {
                Node* CopyofTree = root;
                std::string parentofmynode = "";
                getParentfromtree(CopyofTree, my_curr_node_addr, parentofmynode);
                std::cout << "Going to parent " <<parentofmynode << std::endl;
                std::string file_content        = m_get_file_data.on(get_engine().lookup(parentofmynode))(filename, req_bytes);
                std::cout << "Hop trip my_curr_node_addr " <<my_curr_node_addr << std::endl;
                req.respond(file_content);
            }
        }

        void getParentfromtree(Node* CopyofTree, std::string my_curr_node_addr, std::string &parentofmynode)
        {
            if (my_curr_node_addr == CopyofTree->data)
            {
                parentofmynode= CopyofTree->my_parent->data;
            }
            for (Node* child : CopyofTree->getChildren()) 
            {
                getParentfromtree(child, my_curr_node_addr, parentofmynode);      
            }
        }

        ServerLocalCacheProvider(const ServerLocalCacheProvider&)              = delete;
        ServerLocalCacheProvider(ServerLocalCacheProvider&&)                   = delete;
        ServerLocalCacheProvider& operator = (const ServerLocalCacheProvider&) = delete;
        ServerLocalCacheProvider& operator = (ServerLocalCacheProvider&&)      = delete;

};


void printTree(Node* node) 
{
  std::cout <<" Level " << node->level <<" Child ID at this Level " << node->child_id << " Data " << node->data << std::endl;
  for (Node* child : node->getChildren()) 
  {
    printTree(child);
  }
}

void prettyPrintTree(Node* root, int depth, int dep_counter = 0) 
{
    
    if (root == nullptr) 
    {
        return;
    }

    for (int i = 0; i < depth; i++) 
    {
        std::cout << "    ";
    }

    std::cout <<"(depth "<<dep_counter<<") "<< root->data << std::endl;

    for (Node* child : root->children) 
    {
        prettyPrintTree(child, depth + 1, dep_counter+1);
    }
}

int depth(Node* root) 
{
    if (root == nullptr) 
    {
        return 0;
    }

    int maxDepth = 0;
    for (Node* child : root->children) 
    {
        maxDepth = std::max(maxDepth, depth(child));
    }

    return maxDepth + 1;
}

Node*  build_my_tree(Node* root, std::vector<std::string > node_address_data)
{

    int max_children_per_parent = 1;
    
    // Based on a simple formula for sum of geometric series a(1-r^n)/(1-r) where n = 4 = max level5 -1, a=r=num_children

    if (node_address_data.size() <= 31)         max_children_per_parent = 2;
    else if (node_address_data.size() <= 121)   max_children_per_parent = 3;
    else if (node_address_data.size() <= 341)   max_children_per_parent = 4;
    else if (node_address_data.size() <= 781)   max_children_per_parent = 5;
    else if (node_address_data.size() <= 1555)  max_children_per_parent = 6;
    else  max_children_per_parent = 7;

    // std::cout <<  "max_children_per_parent : " << max_children_per_parent << std::endl; 

    int current_children_per_parent = 0;
    int my_current_level = 0;
    int l1_child_counter=0, l2_child_counter=0, l3_child_counter=0, l4_child_counter=0, l5_child_counter=0;
    int i_id_l2_parent = 0;  // For level 2 children
    int i_id_l3_parent = 0, j_id_l3_parent = 0;  // For level 3 children
    int i_id_l4_parent = 0, j_id_l4_parent = 0, k_id_l4_parent = 0;   // For level 4 children
    int i_id_l5_parent = 0, j_id_l5_parent = 0, k_id_l5_parent = 0, l_id_l5_parent = 0;   // For level 5 children

    float my_current_level_before_trim;

    for (int i_node_counter = 0; i_node_counter < node_address_data.size(); i_node_counter++) 
    {
        if (i_node_counter == 0)
        {
            root = new Node(node_address_data[i_node_counter], nullptr, 0 , 1); // At level 0
            // std::cout << "node counter : " <<i_node_counter << " node ID: " << node_address_data[i_node_counter] << " level : " << my_current_level << std::endl; 

        }
        else
        {
            // To redirect a node to a level  
            // when max_children_per_parent = 2; floor (log (i) / log(2))
            // when max_children_per_parent = 3; floor (log (2 *i) / log(3))
            // when max_children_per_parent = 4; floor (log (3 *i) / log(4))

            my_current_level_before_trim = log ( (max_children_per_parent - 1 ) * i_node_counter+1)  /  log(max_children_per_parent) ;
            my_current_level             = floor ( my_current_level_before_trim ); // Dont touch this - log2(8) is not exactly 3 but only closer to 3

            // std::cout << "node counter : " <<i_node_counter << " node ID: " << node_address_data[i_node_counter] << " level : " << my_current_level << std::endl; 
            

            if (my_current_level == 1)
            {
                l1_child_counter = l1_child_counter +1;
                root->children.push_back(new Node(node_address_data[i_node_counter],root, my_current_level, l1_child_counter));   

                if (l1_child_counter == max_children_per_parent) 
                {
                    l1_child_counter = 0 ; 
                }
            }

            if (my_current_level == 2)
            {
                l2_child_counter = l2_child_counter +1;
                root->children[i_id_l2_parent]->children.push_back(new Node({node_address_data[i_node_counter]}, root->children[i_id_l2_parent], my_current_level, l2_child_counter));     

                if (l2_child_counter == max_children_per_parent) 
                {
                    l2_child_counter = 0 ; 
                    i_id_l2_parent = i_id_l2_parent + 1; 
                }
            }

            if (my_current_level == 3)
            {
                l3_child_counter = l3_child_counter + 1; 
                root->children[i_id_l3_parent]->children[j_id_l3_parent]->children.push_back(new Node({node_address_data[i_node_counter]}, root->children[i_id_l3_parent]->children[j_id_l3_parent], my_current_level, l3_child_counter));    

                if (l3_child_counter == max_children_per_parent) 
                {
                    l3_child_counter = 0 ; 
                    j_id_l3_parent = j_id_l3_parent + 1; 

                    if (j_id_l3_parent == max_children_per_parent)
                    {
                        j_id_l3_parent = 0;
                        i_id_l3_parent = i_id_l3_parent + 1; 
                    }
                }
            }
            
            if (my_current_level == 4) 
            {
                l4_child_counter = l4_child_counter + 1; 
                root->children[i_id_l4_parent]->children[j_id_l4_parent]->children[k_id_l4_parent]->children.push_back(new Node({node_address_data[i_node_counter]}, root->children[i_id_l4_parent]->children[j_id_l4_parent]->children[k_id_l4_parent], my_current_level, l4_child_counter));   

                if (l4_child_counter == max_children_per_parent) 
                {
                    l4_child_counter = 0 ; 
                    k_id_l4_parent = k_id_l4_parent + 1; 

                    if (k_id_l4_parent == max_children_per_parent)
                    {
                        k_id_l4_parent = 0;
                        j_id_l4_parent = j_id_l4_parent + 1; 

                        if (j_id_l4_parent == max_children_per_parent)
                        {
                            j_id_l4_parent = 0;
                            i_id_l4_parent = i_id_l4_parent + 1;                      
                        }
                    }
                }            
            }

            if (my_current_level == 5)  
            {
                l5_child_counter = l5_child_counter + 1; 
                root->children[i_id_l5_parent]->children[j_id_l5_parent]->children[k_id_l5_parent]->children[l_id_l5_parent]->children.push_back(new Node({node_address_data[i_node_counter]}, root->children[i_id_l5_parent]->children[j_id_l5_parent]->children[k_id_l5_parent]->children[l_id_l5_parent], my_current_level, l5_child_counter));   

                if (l5_child_counter == max_children_per_parent) 
                {
                    l5_child_counter = 0 ; 
                    l_id_l5_parent   = l_id_l5_parent + 1; 

                    if (l_id_l5_parent == max_children_per_parent)
                    {
                        l_id_l5_parent = 0;
                        k_id_l5_parent = k_id_l5_parent + 1; 

                        if (k_id_l5_parent == max_children_per_parent)
                        {
                            k_id_l5_parent = 0;
                            j_id_l5_parent = j_id_l5_parent + 1; 

                            if (j_id_l5_parent == max_children_per_parent)
                            {
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

void push_back_with_mutex(std::string  hostname, std::string  my_cxi_server_ip_hex_str) 
{
  
  std::lock_guard<std::mutex> lock(mtx);
  std::ofstream myFile(copper_address_book_name, std::ios_base::app);
  myFile << hostname << " " << my_cxi_server_ip_hex_str << std::endl;
  myFile.close();

}

void get_hsn0_cxi_addr()
{
    std::string my_hsn0_mac_id;
    std::ifstream inFile("/sys/class/net/hsn0/address");
    if (!inFile.is_open()) { std::cerr << "Error opening file" << std::endl; }
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
    std::string my_cxi_server_ip_binary_str = "001"+ my_hsn0_nic_id_bin.to_string() +"000000000";
    // std::cout << my_cxi_server_ip_binary_str << std::endl;
  
    std::bitset<32> my_cxi_server_ip_binary(my_cxi_server_ip_binary_str);  

    std::stringstream my_cxi_server_ip_hex_ss;
    my_cxi_server_ip_hex_ss << std::hex << my_cxi_server_ip_binary.to_ulong();
    std::string my_cxi_server_ip_hex_str = "ofi+cxi://0x"+my_cxi_server_ip_hex_ss.str();
    // std::cout << "my_cxi_server_ip_hex_str " << my_cxi_server_ip_hex_str << std::endl;

    char char_hostname[1024];
    gethostname(char_hostname, sizeof(char_hostname));
    std::string hostname(char_hostname);
    // std::cout << hostname << std::endl;

    // replace mutex with oneapi/tbb/concurrent_vector.h tbb::concurrent_vector is not working currently with oneapi
    push_back_with_mutex(hostname, my_cxi_server_ip_hex_str); 

}

void parse_nodelist_from_cxi_address_book()
{
    sleep(10); //  barrier issue: The first process needs to wait until all the remaining processes have written to the address book. 

    std::ifstream inFile(copper_address_book_name, std::ios::in);

    if (!inFile.is_open()) 
    {
        std::cout  << "Error opening file" << std::endl;
        return ;
    }

    std::string line;

    while (getline(inFile, line))   
    {
        // std::cout  << getpid() << "   "<< line << std::endl;
        size_t pos = line.find(" ");
        std::string first_part_hostname = line.substr(0, pos);
        std::string second_part_cxi     = line.substr(pos + 1);
        // std::cout  << first_part_hostname << second_part_cxi << std::endl;
        global_peer_pairs.push_back(make_pair(first_part_hostname, second_part_cxi)); 
        node_address_data.push_back(second_part_cxi);
    }

    inFile.close();

    // std::cout <<"Total number of compute nodes : " << node_address_data.size() << std::endl ;
    // for (int i = 0; i < node_address_data.size(); i++) 
    // {
    //     std::cout  << "Debug node_address_data from func parse_nodelist_for_cxi " << getpid() << node_address_data[i] << std::endl;
    // }
}

int main(int argc, char** argv) 
{
    char char_hostname[1024];
    gethostname(char_hostname, sizeof(char_hostname));
    std::string hostname(char_hostname);
    std::string curr_node_addr_server;
    std::string curr_node_addr_client;
    std::string per_node_out_file = hostname + "_" +std::string(argv[1]) ;
    std::ofstream out(per_node_out_file);

    // copper_address_book_name = copper_address_book_name + "_" + std::string(argv[1]) + ".txt";

    std::cout.rdbuf(out.rdbuf());

    if (atoi(argv[2]) == 1) 
        get_hsn0_cxi_addr(); 
    

    parse_nodelist_from_cxi_address_book();
    root = build_my_tree(root, node_address_data);
    printTree(root);
    int tree_depth = depth(root);
    std::cout<<"The depth of the tree is :" << tree_depth << std::endl;
    prettyPrintTree(root, tree_depth);


    
    // for (int i = 0; i <  node_address_data.size(); i++) 
    // {
    //     if (global_peer_pairs[i].first == hostname) 
    //     {
    //         // std::cout  << getpid() << hostname<< " "<< std::endl;
    //         curr_node_addr_server =  global_peer_pairs[i].second;
    //         curr_node_addr_client =  global_peer_pairs[i].second;
    //         curr_node_addr_client.replace(curr_node_addr_client.length() - 1, 1, "1");
    //         // std::cout  << getpid() << hostname<< " " 
    //                     //   <<  "curr_node_addr_server " << curr_node_addr_server 
    //                     //   <<  "curr_node_addr_client " << curr_node_addr_client  << std::endl;
    //         break;
    //     }
    // }
 

        auto serverEngine = tl::engine{"cxi", THALLIUM_SERVER_MODE};
        std::cout << "Server running at address " << serverEngine.self() << std::endl;
        serverEngine.enable_remote_shutdown();
        auto get_file_data = serverEngine.define("get_file_data");
        new ServerLocalCacheProvider{serverEngine, node_address_data};
        sleep(10); //  barrier issue: all process need to wait until the server is created.


        // std::cout  <<"Main pid " << getpid() << " " << hostname<< std::endl;
        // {
        //         // thallium::xstream primary = thallium::xstream::self();
        //         // primary.make_thread() ;

        //         auto worker1 = tl::xstream::self().make_thread([serverEngine, get_file_data]() 
        //                 { /* thread function here... */     
        //                     std::thread::id this_tid = std::this_thread::get_id();
        //                     std::cout  <<"Hello from dummy_fuse_func with pid " << getpid()   <<" thread id " << this_tid  << std::endl;
        //                     std::string thread_file_content = get_file_data.on(serverEngine.self())(std::string{"/path/to/file"}, 3);  
        //                     std::cout << "From thread received content : "  << thread_file_content << std::endl;
        //                 });
        // }

            std::string m_my_curr_node_addr = static_cast<std::string>(serverEngine.self());

            if(root->data != m_my_curr_node_addr)
            {

                std::chrono::time_point<std::chrono::system_clock> start, end;
                for (int i = 0; i < 10; i++) 
                {
                    start = std::chrono::system_clock::now();
                    std::string client_file_content = get_file_data.on(serverEngine.self())(std::string{"/path/to/file"}, atoi(argv[1]));  
                    end = std::chrono::system_clock::now();           
                    
                    std::chrono::duration<double> elapsed_seconds = end - start;
                    std::cout << "I'm the main requester " << curr_node_addr_server 
                            << " elapsed time " << elapsed_seconds.count() << " s " << " Bytes " << atoi(argv[1]) << " iter " << i << std::endl ; 
                    // std::cout << "Finally received content : "  << client_file_content << std::endl;
                }
            }

        out.close();

        std::cout << "Waiting on Finalize " << std::endl;
        serverEngine.wait_for_finalize();
        std::cout << "Successfully killed by remote shutdown process - closing down" << std::endl;

    return 0;
}
