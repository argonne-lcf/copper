#include "util.h"

#include "../metric/operations.h"

std::string Util::rel_to_abs_path(const char* path) {
    if(!path) {
        LOG(FATAL) << "path was null" << std::endl;
        throw std::runtime_error("path was null");
    }

    if(!Constants::target_path.has_value()) {
        LOG(FATAL) << Constants::usage << std::endl;
        throw std::runtime_error("-tpath argument not found");
    }

    if(strcmp(path, "/") == 0) {
        LOG(DEBUG) << "operation on /" << std::endl;
        return Constants::target_path.value();
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

std::vector<std::byte> Util::read_ent_file(const std::string& path, bool is_file) {
    if(std::ifstream source_file{path, std::ios::binary}) {
        std::streamsize file_size{};
        file_size = static_cast<std::streamsize>(std::filesystem::file_size(path));
        std::vector<std::byte> bytes(file_size);
        source_file.read(reinterpret_cast<char*>(bytes.data()), file_size);
        source_file.close();

        return bytes;
    }

    throw std::runtime_error("failed to open path: " + path);
}

std::optional<std::ofstream> Util::try_get_fstream_from_path(const char* path) {
    if(path == nullptr) {
        LOG(ERROR) << "cstr path was null" << std::endl;
        return std::nullopt;
    }

    const std::string output_path_string = std::string(path);
    std::ofstream file(output_path_string, std::ios::out | std::ios::trunc);

    if(!file.is_open()) {
        LOG(ERROR) << "failed to open path: " << output_path_string << std::endl;
        return std::nullopt;
    }

    LOG(DEBUG) << "succesfully opened path: " << output_path_string << std::endl;

    return file;
}

std::string Util::get_current_datetime() {
    const time_t now = time(nullptr);
    char buf[80];
    const tm tstruct = *localtime(&now);
    strftime(buf, sizeof(buf), "%Y-%m-%d.%X", &tstruct);

    return buf;
}