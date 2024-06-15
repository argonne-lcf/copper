#include "util.h"

std::string Util::rel_to_abs_path(const char* path) {
    if(!path) {
        LOG(FATAL) << "path was null" << std::endl;
        throw std::runtime_error("path was null");
    }

    const auto suffix{path};
    const auto abs_path{Constants::target_path.value() + suffix};

    return abs_path;
}

std::vector<std::string> Util::process_args(const int argc, const char* argv[]) {
    umask(0);

    auto original_string_args{std::vector<std::string>()};
    for(int i{0}; i < argc; i++) {
        std::string cur_string{argv[i]};
        original_string_args.push_back(cur_string);
    }

    auto new_string_args{std::vector<std::string>()};
    for(int i{0}; i < original_string_args.size();) {
        if(original_string_args[i] == "--plus") {
            Constants::fill_dir_plus = FUSE_FILL_DIR_PLUS;
        } else if(original_string_args[i] == "-tpath") {
            if(i + 2 >= original_string_args.size()) {
                LOG(FATAL) << Constants::usage << std::endl;
                throw std::runtime_error("no argument after -tpath");
            }

            Constants::target_path = std::string(argv[i + 1]);
            LOG(DEBUG) << "-tpath was found: " << Constants::target_path.value() << std::endl;
            i += 2;
        } else {
            new_string_args.push_back(original_string_args[i]);
            i++;
        }
    }
    if(!Constants::target_path.has_value()) {
        LOG(FATAL) << Constants::usage << std::endl;
        throw std::runtime_error("-tpath argument not found");
    }

    return new_string_args;
}
