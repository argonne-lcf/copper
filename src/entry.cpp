#include "fs/cu_fuse.h"

#include <iostream>
#include <string_view>

int main(int argc, char** argv) {
    for(int index = 1; index < argc; ++index) {
        const std::string_view argument(argv[index]);
        if(argument == "-h" || argument == "--help") {
            std::cout << Constants::usage << '\n';
            return 0;
        }
    }
    try {
        return CuFuse::cu_hello_main(argc, argv);
    } catch(const std::exception& error) {
        std::cerr << "cu_fuse: " << error.what() << '\n';
        return 2;
    }
}
