#include "fs/error_status.h"

#include <cerrno>
#include <cstddef>
#include <iostream>
#include <vector>

#define CHECK(expr) do { if(!(expr)) { std::cerr << __FILE__ << ':' << __LINE__ << ": CHECK failed: " #expr "\n"; return 1; } } while(false)

namespace {
struct ReadResult { ssize_t status; std::vector<std::byte> bytes; };
ReadResult failed_read(int saved_errno) { return {CopperError::negative_errno_or_eio(saved_errno), {}}; }
bool cacheable(const ReadResult& result) { return CopperError::is_success(result.status); }
}

int main() {
    for(const auto& result : {failed_read(EIO), failed_read(0), failed_read(ECONNRESET)}) {
        CHECK(result.status < 0);
        CHECK(!cacheable(result));
    }
    CHECK(failed_read(EACCES).status == -EACCES);
    CHECK(failed_read(0).status == -EIO);
    return 0;
}