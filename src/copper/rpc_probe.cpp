#include <thallium.hpp>
#include <thallium/serialization/stl/string.hpp>

#include <cerrno>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>

namespace tl = thallium;

int main(int argc, char** argv) {
    if(argc != 5) {
        std::cerr << "usage: cu_fuse_probe <network-type> <provider-id> <address-book> <hostname>\n";
        return 2;
    }
    const std::string network_type(argv[1]);
    char* end = nullptr;
    errno = 0;
    const auto parsed_provider = std::strtoul(argv[2], &end, 10);
    if(errno != 0 || end == argv[2] || *end != '\0' || parsed_provider > UINT16_MAX) {
        std::cerr << "invalid provider id: " << argv[2] << '\n';
        return 2;
    }
    const auto provider_id = static_cast<uint16_t>(parsed_provider);
    std::ifstream input(argv[3]);
    if(!input) {
        std::cerr << "failed to open address book: " << argv[3] << '\n';
        return 2;
    }
    std::string hostname;
    std::string address;
    std::string requested(argv[4]);
    bool found = false;
    while(input >> hostname >> address) {
        const auto dot = hostname.find('.');
        if(dot != std::string::npos) hostname.resize(dot);
        if(hostname == requested) {
            found = true;
            break;
        }
    }
    if(!found) {
        std::cerr << "hostname absent from address book: " << requested << '\n';
        return 2;
    }
    try {
        auto engine = tl::engine{network_type, THALLIUM_SERVER_MODE};
        auto endpoint = engine.lookup(address);
        tl::provider_handle provider(endpoint, provider_id);
        auto ready_rpc = engine.define("rpc_is_ready");
        const bool ready = ready_rpc.on(provider)();
        engine.finalize();
        return ready ? 0 : 1;
    } catch(const std::exception& error) {
        std::cerr << "readiness RPC failed for " << requested << ": " << error.what() << '\n';
        return 1;
    }
}
