#include <iostream>
#include <thallium.hpp>
#include <unistd.h>  // for gethostname()
#include <limits.h> 
namespace tl = thallium;

int main(int argc, char** argv) {

    tl::engine myEngine("cxi://cxi0", THALLIUM_SERVER_MODE, true,1);
    // server_engine = new tl::engine{"cxi://cxi0:0", THALLIUM_SERVER_MODE, true, Constants::es};

//Constants::network_type, THALLIUM_SERVER_MODE, true, Constants::es};
// ofi+cxi://
// ofi+cxi://
// cxi1

char hostname[HOST_NAME_MAX];

    if (gethostname(hostname, sizeof(hostname)) == 0) {
        std::cout << "Hostname: " << hostname << std::endl;
    } 


    std::cout << "Server " << hostname << "running at address " << myEngine.self() << std::endl;

    myEngine.wait_for_finalize();
    return 0;
}