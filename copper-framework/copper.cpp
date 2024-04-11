#include <thallium.hpp>
#include <thallium/serialization/stl/string.hpp>
#include <array>
#include <iostream>
#include <fstream>
#include <sys/socket.h>
#include <unistd.h> 
#include <string>
#include <vector>
#include <cmath>

namespace tl = thallium;

class Node 
{
    public:
        std::string data;
        std::vector<Node*> children;

    Node(std::string data) 
    {
        this->data = data;
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

void printTree(Node* node) 
{
  std::cout << node->data << std::endl;

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

    std::cout <<  "max_children_per_parent : " << max_children_per_parent << std::endl; 

    int current_children_per_parent = 0;
    int my_current_level = 0;
    int l2_child_counter=0, l3_child_counter=0, l4_child_counter=0, l5_child_counter=0;
    int i_id_l2_parent = 0;  // For level 2 children
    int i_id_l3_parent = 0, j_id_l3_parent = 0;  // For level 3 children
    int i_id_l4_parent = 0, j_id_l4_parent = 0, k_id_l4_parent = 0;   // For level 4 children
    int i_id_l5_parent = 0, j_id_l5_parent = 0, k_id_l5_parent = 0, l_id_l5_parent = 0;   // For level 5 children

    float my_current_level_before_trim;

    for (int i_node_counter = 0; i_node_counter < node_address_data.size(); i_node_counter++) 
    {
        if (i_node_counter == 0)
        {
            root = new Node(node_address_data[i_node_counter]); // At level 0
        }
        else
        {
            // To redirect a node to a level  
            // when max_children_per_parent = 2; floor (log (i) / log(2))
            // when max_children_per_parent = 3; floor (log (2 *i) / log(3))
            // when max_children_per_parent = 4; floor (log (3 *i) / log(4))

            my_current_level_before_trim = log ( (max_children_per_parent - 1 ) * i_node_counter+1)  /  log(max_children_per_parent) ;
            my_current_level             = floor ( my_current_level_before_trim ); // Dont touch this - log2(8) is not exactly 3 but only closer to 3

            std::cout << "node id : " <<i_node_counter << " level : " << my_current_level << std::endl; 
            

            if (my_current_level == 1)
            {
                root->children.push_back(new Node(node_address_data[i_node_counter]));   
            }

            if (my_current_level == 2)
            {
                root->children[i_id_l2_parent]->children.push_back(new Node{node_address_data[i_node_counter]});     
                l2_child_counter = l2_child_counter +1;

                if (l2_child_counter == max_children_per_parent) 
                {
                    l2_child_counter = 0 ; 
                    i_id_l2_parent = i_id_l2_parent + 1; 
                }
            }

            if (my_current_level == 3)
            {
                root->children[i_id_l3_parent]->children[j_id_l3_parent]->children.push_back(new Node{node_address_data[i_node_counter]});    
                l3_child_counter = l3_child_counter + 1; 

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
                root->children[i_id_l4_parent]->children[j_id_l4_parent]->children[k_id_l4_parent]->children.push_back(new Node{node_address_data[i_node_counter]});   
                l4_child_counter = l4_child_counter + 1; 

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
                root->children[i_id_l5_parent]->children[j_id_l5_parent]->children[k_id_l5_parent]->children[l_id_l5_parent]->children.push_back(new Node{node_address_data[i_node_counter]});   
                l5_child_counter = l5_child_counter + 1; 

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


void parse_nodelist(std::vector<std::string >& node_address_data, std::string pbs_nodelist_file)
{
    std::ifstream inFile(pbs_nodelist_file);

    if (!inFile.is_open()) 
    {
        std::cout  << "Error opening file" << std::endl;
        return ;
    }

    std::string line;
    std::string delimiter = ".";

    while (getline(inFile, line))   
    {
        // std::cout  << line << std::endl;
        size_t pos = line.find(delimiter);
        std::string first_part = line.substr(0, pos);
        first_part = first_part + "-hsn0:10001";
        // std::cout  << first_part << std::endl;
        node_address_data.push_back(first_part);
    }

    inFile.close();

    std::cout <<"Total number of compute nodes : " << node_address_data.size() << std::endl ;
    for (int i = 0; i < node_address_data.size(); i++) 
    {
        std::cout  << node_address_data[i] << std::endl;
    }
}


class ServerLocalCacheProvider : public tl::provider<ServerLocalCacheProvider> 
{

    std::vector<tl::endpoint> m_peers;
    tl::remote_procedure      m_get_file_data;

    public:

        ServerLocalCacheProvider(tl::engine serverEngine, const std::vector<std::string>& addresses) : 
        tl::provider<ServerLocalCacheProvider>{std::move(serverEngine), 0} , m_get_file_data{define("get_file_data", &ServerLocalCacheProvider::getFileData)}
        {
            // Setup finalization callback
            get_engine().push_finalize_callback([this](){ delete this;});

            // Lookup peers
            m_peers.reserve(addresses.size());

            for(auto& address : addresses) 
            {
                m_peers.push_back(get_engine().lookup(address));
            }

        }

        void getFileData(const tl::request& req, const std::string& filename) 
        {
            std::cout << "Client at address " << static_cast<std::string>(req.get_endpoint())  << " requested data for file " << filename << std::endl;
            std::string file_content = "Hello World From Server";
            req.respond(file_content);

        }

        ServerLocalCacheProvider(const ServerLocalCacheProvider&)              = delete;
        ServerLocalCacheProvider(ServerLocalCacheProvider&&)                   = delete;
        ServerLocalCacheProvider& operator = (const ServerLocalCacheProvider&) = delete;
        ServerLocalCacheProvider& operator = (ServerLocalCacheProvider&&)      = delete;

};


int main(int argc, char** argv) 
{

    char char_hostname[1024];
    gethostname(char_hostname, sizeof(char_hostname));
    std::string hostname(char_hostname);

    // std::string hostname;
    std::cout << "Hostname: " << hostname << std::endl;
    size_t pos = hostname.find(".");
    std::string curr_node_addr = hostname.substr(0, pos);

    std::vector<std::string > node_address_data;
    std::string pbs_nodelist_file = "sample_input/pbs8.txt";


    if (argc == 2)   // no arguments were passed
    {
       pbs_nodelist_file = argv[1];
    }

    std::cout<< "PBS_NODEFILE_NAME is : " <<  pbs_nodelist_file << std::endl;

    parse_nodelist(node_address_data, pbs_nodelist_file);
    int total_nodes = node_address_data.size();



    Node* root;
    root = build_my_tree(root, node_address_data);
    // printTree(root);
    int tree_depth = depth(root);
    std::cout<<"The depth of the tree is :" << tree_depth << std::endl;
    prettyPrintTree(root, tree_depth);




    // Begin Server code 

        std::string curr_node_addr_server = "cxi://" + curr_node_addr + "-hsn0:10001";
        auto serverEngine = tl::engine{curr_node_addr_server, THALLIUM_SERVER_MODE};

        serverEngine.enable_remote_shutdown();

        auto val_server_address = static_cast<std::string>(serverEngine.self());
        std::cout<< val_server_address << std::endl;

        // std::vector<std::string> addresses(size);

        new ServerLocalCacheProvider{serverEngine, node_address_data};

        // Run progress loop
        serverEngine.wait_for_finalize();

    // End Server code 




    // Begin client code 
        
        //.hsn.cm.americas.sgi.com
        std::string curr_node_addr_client = "cxi://" + curr_node_addr + "-hsn0:10002";
        auto clientEngine = tl::engine{curr_node_addr_client, THALLIUM_SERVER_MODE};
        auto get_file_data = clientEngine.define("get_file_data");

        auto val_client_address = static_cast<std::string>(clientEngine.self());
        std::cout<< val_client_address << std::endl;

        {
            auto local_server_endpoint = clientEngine.lookup(curr_node_addr_server);
            // Send an RPC to get the content of the data
            std::string client_file_content = get_file_data.on(local_server_endpoint)(std::string{"/path/to/file"});
            std::cout << "Server " << curr_node_addr_server << " responded to file content: "  << client_file_content << std::endl;

            clientEngine.shutdown_remote_engine(local_server_endpoint);
        }

        // Finalize the server.
        serverEngine.finalize();

    // End client code 



    return 0;
}