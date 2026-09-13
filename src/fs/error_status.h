#ifndef COPPER_ERROR_STATUS_H
#define COPPER_ERROR_STATUS_H
#include <cerrno>
namespace CopperError {
inline int negative_errno_or_eio(int error) { return error > 0 ? -error : -EIO; }
inline bool is_success(int status) { return status >= 0; }
}
#endif