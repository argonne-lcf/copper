#include "operations.h"

template <typename t>
static void
log_operation_helper(const std::string& table_name, std::ostream& os, t (&table_array)[static_cast<int>(OperationFunction::size)]) {
#ifdef COLLECT_METRICS
    os << Util::get_current_datetime() << std::endl;

    os << table_name << " {" << std::endl;
    os << "getattr: " << table_array[static_cast<t>(OperationFunction::getattr)] << "," << std::endl;
    os << "readlink: " << table_array[static_cast<t>(OperationFunction::readlink)] << "," << std::endl;
    os << "mknod: " << table_array[static_cast<t>(OperationFunction::mknod)] << "," << std::endl;
    os << "mkdir: " << table_array[static_cast<t>(OperationFunction::mkdir)] << "," << std::endl;
    os << "unlink: " << table_array[static_cast<t>(OperationFunction::unlink)] << "," << std::endl;
    os << "rmdir: " << table_array[t(OperationFunction::rmdir)] << "," << std::endl;
    os << "symlink: " << table_array[t(OperationFunction::symlink)] << "," << std::endl;
    os << "rename: " << table_array[t(OperationFunction::rename)] << "," << std::endl;
    os << "link: " << table_array[t(OperationFunction::link)] << "," << std::endl;
    os << "chmod: " << table_array[t(OperationFunction::chmod)] << "," << std::endl;
    os << "chown: " << table_array[t(OperationFunction::chown)] << "," << std::endl;
    os << "truncate: " << table_array[t(OperationFunction::truncate)] << "," << std::endl;
    os << "open: " << table_array[t(OperationFunction::open)] << "," << std::endl;
    os << "read: " << table_array[t(OperationFunction::read)] << "," << std::endl;
    os << "write: " << table_array[t(OperationFunction::write)] << "," << std::endl;
    os << "statfs: " << table_array[t(OperationFunction::statfs)] << "," << std::endl;
    os << "flush: " << table_array[t(OperationFunction::flush)] << "," << std::endl;
    os << "release: " << table_array[t(OperationFunction::release)] << "," << std::endl;
    os << "fsync: " << table_array[t(OperationFunction::fsync)] << "," << std::endl;
    os << "setxattr: " << table_array[t(OperationFunction::setxattr)] << "," << std::endl;
    os << "getxattr: " << table_array[t(OperationFunction::getxattr)] << "," << std::endl;
    os << "listxattr: " << table_array[t(OperationFunction::listxattr)] << "," << std::endl;
    os << "removexattr: " << table_array[t(OperationFunction::removexattr)] << "," << std::endl;
    os << "opendir: " << table_array[t(OperationFunction::opendir)] << "," << std::endl;
    os << "readdir: " << table_array[t(OperationFunction::readdir)] << "," << std::endl;
    os << "releasedir: " << table_array[t(OperationFunction::releasedir)] << "," << std::endl;
    os << "fsyncdir: " << table_array[t(OperationFunction::fsyncdir)] << "," << std::endl;
    os << "init: " << table_array[t(OperationFunction::init)] << "," << std::endl;
    os << "destroy: " << table_array[t(OperationFunction::destroy)] << "," << std::endl;
    os << "access: " << table_array[t(OperationFunction::access)] << "," << std::endl;
    os << "create: " << table_array[t(OperationFunction::create)] << "," << std::endl;
    os << "lock: " << table_array[t(OperationFunction::lock)] << "," << std::endl;
    os << "utimens: " << table_array[t(OperationFunction::utimens)] << "," << std::endl;
    os << "bmap: " << table_array[t(OperationFunction::bmap)] << "," << std::endl;
    os << "ioctl: " << table_array[t(OperationFunction::ioctl)] << "," << std::endl;
    os << "poll: " << table_array[t(OperationFunction::poll)] << "," << std::endl;
    os << "write_buf: " << table_array[t(OperationFunction::write_buf)] << "," << std::endl;
    os << "read_buf: " << table_array[t(OperationFunction::read_buf)] << "," << std::endl;
    os << "flock: " << table_array[t(OperationFunction::flock)] << "," << std::endl;
    os << "fallocate: " << table_array[t(OperationFunction::fallocate)] << "," << std::endl;
    os << "copy_file_range: " << table_array[t(OperationFunction::copy_file_range)] << "," << std::endl;
    os << "lseek: " << table_array[t(OperationFunction::lseek)] << std::endl;
    os << "}";
#endif
}

void Operations::inc_operation(OperationFunction func) {
#ifdef COLLECT_METRICS
    operation_counter[static_cast<int>(func)]++;
#endif
}

void Operations::inc_operation_timer(OperationFunction func, const long time) {
#ifdef COLLECT_METRICS
    operation_timer[static_cast<long>(func)] += time;
#endif
}

void Operations::inc_operation_cache_hit(OperationFunction func, const bool cache_hit) {
#ifdef COLLECT_METRICS
    if(cache_hit) {
        operation_cache_hit[static_cast<int>(func)]++;
    } else {
        operation_cache_miss[static_cast<int>(func)]++;
    }
#endif
}

std::ostream& Operations::log_operation(std::ostream& os) {
    log_operation_helper<int>("operation_counter", os, operation_counter);
    return os;
}

std::ostream& Operations::log_operation_time(std::ostream& os) {
    log_operation_helper<long>("operation_timer (microseconds)", os, operation_timer);
    return os;
}

std::ostream& Operations::log_operation_cache_hit(std::ostream& os) {
    log_operation_helper<int>("operation_cache_hit", os, operation_cache_hit);
    return os;
}

std::ostream& Operations::log_operation_cache_miss(std::ostream& os) {
    log_operation_helper<int>("operation_cache_miss", os, operation_cache_miss);
    return os;
}