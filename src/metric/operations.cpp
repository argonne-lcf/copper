#include "operations.h"

void Operations::inc_operation(OperationFunction func) {
    Operations::operation_counter[static_cast<int>(func)]++;
}

std::ostream& Operations::log(std::ostream& os) {
    os << "operations: {" << std::endl;
    os << "getattr: " << Operations::operation_counter[static_cast<int>(OperationFunction::getattr)] << std::endl;
    os << "readlink: " << Operations::operation_counter[static_cast<int>(OperationFunction::readlink)] << std::endl;
    os << "mknod: " << Operations::operation_counter[static_cast<int>(OperationFunction::mknod)] << std::endl;
    os << "mkdir: " << Operations::operation_counter[static_cast<int>(OperationFunction::mkdir)] << std::endl;
    os << "unlink: " << Operations::operation_counter[static_cast<int>(OperationFunction::unlink)] << std::endl;
    os << "rmdir: " << Operations::operation_counter[static_cast<int>(OperationFunction::rmdir)] << std::endl;
    os << "symlink: " << Operations::operation_counter[static_cast<int>(OperationFunction::symlink)] << std::endl;
    os << "rename: " << Operations::operation_counter[static_cast<int>(OperationFunction::rename)] << std::endl;
    os << "link: " << Operations::operation_counter[static_cast<int>(OperationFunction::link)] << std::endl;
    os << "chmod: " << Operations::operation_counter[static_cast<int>(OperationFunction::chmod)] << std::endl;
    os << "chown: " << Operations::operation_counter[static_cast<int>(OperationFunction::chown)] << std::endl;
    os << "truncate: " << Operations::operation_counter[static_cast<int>(OperationFunction::truncate)] << std::endl;
    os << "open: " << Operations::operation_counter[static_cast<int>(OperationFunction::open)] << std::endl;
    os << "read: " << Operations::operation_counter[static_cast<int>(OperationFunction::read)] << std::endl;
    os << "write: " << Operations::operation_counter[static_cast<int>(OperationFunction::write)] << std::endl;
    os << "statfs: " << Operations::operation_counter[static_cast<int>(OperationFunction::statfs)] << std::endl;
    os << "flush: " << Operations::operation_counter[static_cast<int>(OperationFunction::flush)] << std::endl;
    os << "release: " << Operations::operation_counter[static_cast<int>(OperationFunction::release)] << std::endl;
    os << "fsync: " << Operations::operation_counter[static_cast<int>(OperationFunction::fsync)] << std::endl;
    os << "setxattr: " << Operations::operation_counter[static_cast<int>(OperationFunction::setxattr)] << std::endl;
    os << "getxattr: " << Operations::operation_counter[static_cast<int>(OperationFunction::getxattr)] << std::endl;
    os << "listxattr: " << Operations::operation_counter[static_cast<int>(OperationFunction::listxattr)] << std::endl;
    os << "removexattr: " << Operations::operation_counter[static_cast<int>(OperationFunction::removexattr)] << std::endl;
    os << "opendir: " << Operations::operation_counter[static_cast<int>(OperationFunction::opendir)] << std::endl;
    os << "readdir: " << Operations::operation_counter[static_cast<int>(OperationFunction::readdir)] << std::endl;
    os << "releasedir: " << Operations::operation_counter[static_cast<int>(OperationFunction::releasedir)] << std::endl;
    os << "fsyncdir: " << Operations::operation_counter[static_cast<int>(OperationFunction::fsyncdir)] << std::endl;
    os << "init: " << Operations::operation_counter[static_cast<int>(OperationFunction::init)] << std::endl;
    os << "destroy: " << Operations::operation_counter[static_cast<int>(OperationFunction::destroy)] << std::endl;
    os << "access: " << Operations::operation_counter[static_cast<int>(OperationFunction::access)] << std::endl;
    os << "create: " << Operations::operation_counter[static_cast<int>(OperationFunction::create)] << std::endl;
    os << "lock: " << Operations::operation_counter[static_cast<int>(OperationFunction::lock)] << std::endl;
    os << "utimens: " << Operations::operation_counter[static_cast<int>(OperationFunction::utimens)] << std::endl;
    os << "bmap: " << Operations::operation_counter[static_cast<int>(OperationFunction::bmap)] << std::endl;
    os << "ioctl: " << Operations::operation_counter[static_cast<int>(OperationFunction::ioctl)] << std::endl;
    os << "poll: " << Operations::operation_counter[static_cast<int>(OperationFunction::poll)] << std::endl;
    os << "write_buf: " << Operations::operation_counter[static_cast<int>(OperationFunction::write_buf)] << std::endl;
    os << "read_buf: " << Operations::operation_counter[static_cast<int>(OperationFunction::read_buf)] << std::endl;
    os << "flock: " << Operations::operation_counter[static_cast<int>(OperationFunction::flock)] << std::endl;
    os << "fallocate: " << Operations::operation_counter[static_cast<int>(OperationFunction::fallocate)] << std::endl;
    os << "copy_file_range: " << Operations::operation_counter[static_cast<int>(OperationFunction::copy_file_range)] << std::endl;
    os << "lseek: " << Operations::operation_counter[static_cast<int>(OperationFunction::lseek)] << std::endl;
    os << "}" << std::endl;

    return os;
}
