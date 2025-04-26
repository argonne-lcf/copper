
#include <assert.h>
#include <stdio.h>
#include <margo.h>

#include <unistd.h>

int main(int argc, char** argv)
{
    margo_instance_id mid = margo_init("cxi", MARGO_SERVER_MODE, 0, -1);
    // margo_instance_id mid = margo_init("cxi://cxi0:0", MARGO_SERVER_MODE, 0, -1);

    assert(mid);
    hg_addr_t my_address;
    margo_addr_self(mid, &my_address);
    char addr_str[128];
    char hostname[1024];

    size_t addr_str_size = 128;
    margo_addr_to_string(mid, addr_str, &addr_str_size, my_address);
    margo_addr_free(mid,my_address);

    if (gethostname(hostname, sizeof(hostname)) == 0) {
        printf("Hostname: %s\n", hostname);
    } else {
        perror("gethostname");
        return 1;
    }

    margo_set_log_level(mid, MARGO_LOG_INFO);
    margo_info(mid, "Server running at address %s %s", hostname, addr_str);

    margo_wait_for_finalize(mid);

    return 0;
}