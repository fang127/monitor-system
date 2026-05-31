#pragma once

#include <cstdint>
#include <string>

namespace monitor {

struct WorkerIdentity {
    std::string worker_id;
    std::string worker_token;
};

struct WorkerScope {
    std::string tenant_id;
    std::string team_id;
    std::string cluster_id;
    std::uint64_t server_id = 0;
};

} // namespace monitor
