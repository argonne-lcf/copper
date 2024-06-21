#include "util.h"

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

// FIXME: cache file data here
static void recurse_target_path() {
    for(const auto& entry : std::filesystem::recursive_directory_iterator(Constants::target_path.value())) {
        const std::filesystem::path& entry_path = entry.path();
        const std::filesystem::path parent_path = entry_path.parent_path();
        std::string entry_string = entry_path.string();
        std::string parent_string = parent_path.string();

        LOG(DEBUG) << "found entry_path: " << entry_path << std::endl;
        LOG(DEBUG) << "adding to metadata cache" << std::endl;
        CuStat cu_stat{};
        if(stat(entry_path.c_str(), cu_stat.get_st()) == -1) {
            LOG(FATAL) << "unable to stat entry_path: " << entry_path << std::endl;
            throw std::runtime_error("unable to stat entry_path");
        } else {
            CurCache::md_cache_table.put_force(entry_string, std::move(cu_stat));
        }

        LOG(DEBUG) << "adding to tree location to cache" << std::endl;
        TreeCacheTable::add_to_tree_cache(entry_string, !is_directory(entry_path));

        if(!is_directory(entry_path)) {
            LOG(DEBUG) << "was a file adding to data cached" << std::endl;
            auto bytes = Util::read_ent_file(entry_path.string(), false);
            CurCache::data_cache_table.put_force(entry_path.string(), std::move(bytes));
        }
    }
}

void Util::cache_target_path() {
    if(!Constants::target_path.has_value()) {
        LOG(FATAL) << "target_path had no value" << std::endl;
        throw std::runtime_error("target_path had no value");
    }

    auto target_path_string{Constants::target_path.value()};
    CuStat cu_stat{};

    if(stat(target_path_string.c_str(), cu_stat.get_st()) == -1) {
        LOG(FATAL) << "failed to stat target_path" << std::endl;
        throw std::runtime_error("failed to stat target_path");
    } else {
        LOG(DEBUG) << "cached target_path metdata" << std::endl;
    }

    CurCache::md_cache_table.put_force(target_path_string, std::move(cu_stat));
    CurCache::tree_cache_table.put_force(target_path_string, std::vector<std::string>());

    recurse_target_path();

    LOG(TRACE) << CurCache::md_cache_table << std::endl;
    LOG(TRACE) << CurCache::data_cache_table << std::endl;
    LOG(TRACE) << CurCache::tree_cache_table << std::endl;
}

char* Util::deep_cpy_string(const std::string& str) {
    const std::string::size_type size = str.size();
    const auto entry_name_cstr = new char[size + 1];
    memcpy(entry_name_cstr, str.c_str(), size + 1);

    return entry_name_cstr;
}

std::string Util::get_base_of_path(const std::string& str) {
    auto base_name = std::filesystem::path(str).filename();

    if(base_name.empty()) {
        LOG(ERROR) << "failed to get base_name for path: " << str << std::endl;
        throw std::runtime_error("failed to get base_name for path");
    }

    return base_name;
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

int Util::gen_inode() {
    std::random_device rd;
    std::mt19937_64 gen(rd());
    std::uniform_int_distribution<int> dis;

    return dis(gen);
}

void Util::remove_entry_from_cache(std::string path) {
    const std::string parent_path = std::filesystem::path(path).parent_path().string();
    CurCache::md_cache_table.remove(path);

    if(Util::is_dir(path)) {
        auto children_opt = CurCache::tree_cache_table.get(path);

        if(!children_opt.has_value()) {
            LOG(ERROR) << "failed to find children vector for path: " << path << std::endl;
            return;
        }

        auto children = *children_opt.value();

        for(const auto& child : children) {
            remove_entry_from_cache(child);
        }

        CurCache::tree_cache_table.remove(path);
    } else {
        CurCache::data_cache_table.remove(path);
    }

    auto parent_children_opt = CurCache::tree_cache_table.get(parent_path);

    if(!parent_children_opt.has_value()) {
        LOG(ERROR) << "failed to find parent path: " << parent_path << std::endl;
        return;
    }

    std::vector<std::string>* parent_children = parent_children_opt.value();
    parent_children->clear();
    auto it = std::find(parent_children->begin(), parent_children->end(), path);
    if(it != parent_children->end()) {
        parent_children->erase(it);
    }
}

bool Util::is_dir(std::string path) {
    auto entry = CurCache::data_cache_table.get(path);
    return !entry.has_value();
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
