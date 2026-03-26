// Helper used by version9 discover-address mode.
// It prints one row per host with:
// - hostname
// - HSN IPv4 addresses
// - HSN MAC addresses
// - Thallium CXI endpoints derived on that host

#include <ifaddrs.h>
#include <limits.h>
#include <netpacket/packet.h>
#include <net/if.h>
#include <arpa/inet.h>
#include <mpi.h>
#include <sys/types.h>
#include <unistd.h>

#include <iomanip>
#include <numeric>
#include <sstream>
#include <cstring>
#include <iostream>
#include <map>
#include <regex>
#include <algorithm>
#include <string>
#include <vector>

#include <thallium.hpp>

namespace tl = thallium;

struct HsnInfo {
    std::string ipv4;
    std::string mac;
};

static std::string get_hostname() {
    char hostname[HOST_NAME_MAX] = {0};
    if (gethostname(hostname, sizeof(hostname)) != 0) {
        return "unknown-host";
    }
    hostname[sizeof(hostname) - 1] = '\0';
    return std::string(hostname);
}

static std::string format_mac_address(const unsigned char* addr, int len) {
    if (addr == nullptr || len <= 0) {
        return "NA";
    }

    std::ostringstream out;
    out << std::hex << std::setfill('0');
    for (int i = 0; i < len; ++i) {
        if (i != 0) {
            out << ':';
        }
        out << std::setw(2) << static_cast<unsigned int>(addr[i]);
    }
    return out.str();
}

static std::map<int, HsnInfo> get_hsn_interfaces() {
    std::map<int, HsnInfo> interfaces;
    struct ifaddrs* ifaddr = nullptr;
    const std::regex hsn_regex("^hsn([0-9]+)$");

    if (getifaddrs(&ifaddr) != 0) {
        return {};
    }

    // Collect IPv4 and MAC data for each hsn<N> interface that is currently up.
    for (struct ifaddrs* ifa = ifaddr; ifa != nullptr; ifa = ifa->ifa_next) {
        if (ifa->ifa_addr == nullptr || ifa->ifa_name == nullptr) {
            continue;
        }
        if ((ifa->ifa_flags & IFF_UP) == 0) {
            continue;
        }

        std::string name(ifa->ifa_name);
        std::smatch match;
        if (!std::regex_match(name, match, hsn_regex)) {
            continue;
        }

        int index = std::stoi(match[1].str());
        HsnInfo& info = interfaces[index];

        if (ifa->ifa_addr->sa_family == AF_INET) {
            char addr_buf[INET_ADDRSTRLEN] = {0};
            auto* sa = reinterpret_cast<struct sockaddr_in*>(ifa->ifa_addr);
            if (inet_ntop(AF_INET, &(sa->sin_addr), addr_buf, sizeof(addr_buf)) != nullptr &&
                info.ipv4.empty()) {
                info.ipv4 = addr_buf;
            }
        }

        if (ifa->ifa_addr->sa_family == AF_PACKET) {
            auto* sa = reinterpret_cast<struct sockaddr_ll*>(ifa->ifa_addr);
            if (sa->sll_halen > 0 && info.mac.empty()) {
                info.mac = format_mac_address(sa->sll_addr, sa->sll_halen);
            }
        }
    }

    freeifaddrs(ifaddr);
    return interfaces;
}

static std::string try_cxi_address(int index) {
    try {
        // Build the same style of endpoint string Copper uses at runtime so the
        // discover wrapper can later select the right network column.
        std::string protocol = "cxi://cxi" + std::to_string(index);
        tl::engine engine(protocol, THALLIUM_SERVER_MODE, true, 1);
        return static_cast<std::string>(engine.self());
    } catch (const std::exception& ex) {
        return "NA";
    }
}

int main() {
    MPI_Init(nullptr, nullptr);

    int rank = 0;
    int size = 1;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    const std::string hostname = get_hostname();
    const std::map<int, HsnInfo> hsn_interfaces = get_hsn_interfaces();

    int local_max_index = 0;
    if (!hsn_interfaces.empty()) {
        local_max_index = hsn_interfaces.rbegin()->first;
    }

    int global_max_index = 0;
    MPI_Allreduce(&local_max_index, &global_max_index, 1, MPI_INT, MPI_MAX, MPI_COMM_WORLD);

    // Emit one compact pipe-delimited row per host so the wrapper can later
    // post-process this into Copper's simpler "hostname endpoint" file.
    std::ostringstream row;
    row << hostname;
    for (int index = 0; index <= global_max_index; ++index) {
        const auto hsn_it = hsn_interfaces.find(index);
        const std::string hsn_value =
            (hsn_it != hsn_interfaces.end() && !hsn_it->second.ipv4.empty()) ? hsn_it->second.ipv4 : "NA";
        const std::string mac_value =
            (hsn_it != hsn_interfaces.end() && !hsn_it->second.mac.empty()) ? hsn_it->second.mac : "NA";
        const std::string cxi_value = try_cxi_address(index);
        row << " | " << hsn_value << " | " << mac_value << " | " << cxi_value;
    }
    std::string local_row = row.str();
    int local_len = static_cast<int>(local_row.size());

    std::vector<int> recv_counts;
    if (rank == 0) {
        recv_counts.resize(size, 0);
    }
    MPI_Gather(&local_len, 1, MPI_INT, recv_counts.data(), 1, MPI_INT, 0, MPI_COMM_WORLD);

    std::vector<int> displs;
    std::vector<char> recv_buffer;
    if (rank == 0) {
        displs.resize(size, 0);
        for (int i = 1; i < size; ++i) {
            displs[i] = displs[i - 1] + recv_counts[i - 1];
        }
        int total = std::accumulate(recv_counts.begin(), recv_counts.end(), 0);
        recv_buffer.resize(total);
    }

    MPI_Gatherv(
        local_row.data(),
        local_len,
        MPI_CHAR,
        recv_buffer.data(),
        recv_counts.data(),
        displs.data(),
        MPI_CHAR,
        0,
        MPI_COMM_WORLD);

    if (rank == 0) {
        std::vector<std::string> rows;
        rows.reserve(size);
        for (int i = 0; i < size; ++i) {
            rows.emplace_back(recv_buffer.data() + displs[i], recv_counts[i]);
        }
        std::sort(rows.begin(), rows.end());

        std::cout << "hostname";
        for (int index = 0; index <= global_max_index; ++index) {
            std::cout << " | hsn" << index << " | mac" << index << " | cxi" << index;
        }
        std::cout << '\n';

        for (const auto& line : rows) {
            std::cout << line << '\n';
        }
    }

    MPI_Finalize();
    return 0;
}
