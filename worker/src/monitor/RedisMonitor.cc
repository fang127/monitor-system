#include "RedisMonitor.h"

#include <sw/redis++/redis++.h>

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

namespace monitor {
namespace {

std::string getEnv(const char *name, const std::string &fallback = "") {
    const char *value = std::getenv(name);
    return value ? std::string(value) : fallback;
}

bool isTruthy(const std::string &value) {
    std::string normalized = value;
    std::transform(normalized.begin(), normalized.end(), normalized.begin(),
                   [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
    return normalized == "1" || normalized == "true" || normalized == "yes" || normalized == "on";
}

uint32_t parsePort(const std::string &value, uint32_t fallback) {
    if (value.empty()) return fallback;
    try {
        int parsed = std::stoi(value);
        return parsed > 0 ? static_cast<uint32_t>(parsed) : fallback;
    } catch (...) {
        return fallback;
    }
}

unsigned int parseTimeout(const std::string &value, unsigned int fallback) {
    if (value.empty()) return fallback;
    try {
        int parsed = std::stoi(value);
        return parsed > 0 ? static_cast<unsigned int>(parsed) : fallback;
    } catch (...) {
        return fallback;
    }
}

uint64_t toUInt64(const std::string &value) {
    if (value.empty()) return 0;
    try {
        return static_cast<uint64_t>(std::stoull(value));
    } catch (...) {
        return 0;
    }
}

double toDouble(const std::string &value) {
    if (value.empty()) return 0.0;
    try {
        return std::stod(value);
    } catch (...) {
        return 0.0;
    }
}

std::vector<std::string> split(const std::string &value, char delimiter) {
    std::vector<std::string> parts;
    std::string item;
    std::istringstream iss(value);
    while (std::getline(iss, item, delimiter)) parts.push_back(item);
    return parts;
}

std::map<std::string, std::string> parseInfo(const std::string &text) {
    std::map<std::string, std::string> values;
    std::istringstream input(text);
    std::string line;
    while (std::getline(input, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty() || line[0] == '#') continue;
        const std::size_t pos = line.find(':');
        if (pos == std::string::npos) continue;
        values[line.substr(0, pos)] = line.substr(pos + 1);
    }
    return values;
}

uint64_t infoUInt64(const std::map<std::string, std::string> &values, const std::string &key) {
    auto it = values.find(key);
    return it == values.end() ? 0 : toUInt64(it->second);
}

double infoDouble(const std::map<std::string, std::string> &values, const std::string &key) {
    auto it = values.find(key);
    return it == values.end() ? 0.0 : toDouble(it->second);
}

std::string infoString(const std::map<std::string, std::string> &values, const std::string &key,
                       const std::string &fallback = "") {
    auto it = values.find(key);
    return it == values.end() ? fallback : it->second;
}

} // namespace

RedisMonitor::RedisMonitor() {
    const std::string targetConfig = getEnv("REDIS_MONITOR_TARGETS");
    enabled_ = isTruthy(getEnv("REDIS_MONITOR_ENABLED")) || !targetConfig.empty();
    timeoutSeconds_ = parseTimeout(getEnv("REDIS_MONITOR_TIMEOUT_SECONDS"), 3);

    if (!targetConfig.empty()) {
        for (const std::string &entry : split(targetConfig, ';')) {
            if (entry.empty()) continue;
            std::vector<std::string> parts = split(entry, '|');
            Target target;
            if (parts.size() > 0) target.instance = parts[0];
            if (parts.size() > 1 && !parts[1].empty()) target.host = parts[1];
            if (parts.size() > 2) target.port = parsePort(parts[2], target.port);
            if (parts.size() > 3) target.password = parts[3];
            if (target.instance.empty()) target.instance = target.host + ":" + std::to_string(target.port);
            targets_.push_back(target);
        }
    } else if (enabled_) {
        Target target;
        target.host = getEnv("REDIS_MONITOR_HOST", target.host);
        target.port = parsePort(getEnv("REDIS_MONITOR_PORT"), target.port);
        target.password = getEnv("REDIS_MONITOR_PASSWORD");
        target.instance = getEnv("REDIS_MONITOR_INSTANCE", target.host + ":" + std::to_string(target.port));
        targets_.push_back(target);
    }

    if (enabled_) std::cout << "RedisMonitor enabled, targets: " << targets_.size() << std::endl;
}

void RedisMonitor::updateOnce(monitor::proto::MonitorInfo *monitorInfo) {
    if (!enabled_ || !monitorInfo) return;

    for (const Target &target : targets_) {
        auto *info = monitorInfo->add_redis_info();
        info->set_instance(target.instance);
        info->set_host(target.host);
        info->set_port(target.port);
        info->set_role("unknown");

        try {
            sw::redis::ConnectionOptions options;
            options.host = target.host;
            options.port = static_cast<int>(target.port);
            if (!target.password.empty()) options.password = target.password;
            options.connect_timeout = std::chrono::milliseconds(timeoutSeconds_ * 1000);
            options.socket_timeout = std::chrono::milliseconds(timeoutSeconds_ * 1000);

            sw::redis::Redis redis(options);
            const std::string infoText = redis.info();
            const auto values = parseInfo(infoText);
            info->set_up(true);

            info->set_version(infoString(values, "redis_version"));
            info->set_role(infoString(values, "role", "unknown"));
            info->set_uptime_in_seconds(infoUInt64(values, "uptime_in_seconds"));
            info->set_connected_clients(infoUInt64(values, "connected_clients"));
            info->set_blocked_clients(infoUInt64(values, "blocked_clients"));
            info->set_maxclients(infoUInt64(values, "maxclients"));
            info->set_used_memory(infoUInt64(values, "used_memory"));
            info->set_maxmemory(infoUInt64(values, "maxmemory"));
            info->set_mem_fragmentation_ratio(infoDouble(values, "mem_fragmentation_ratio"));
            info->set_total_commands_processed(infoUInt64(values, "total_commands_processed"));
            info->set_instantaneous_ops_per_sec(infoDouble(values, "instantaneous_ops_per_sec"));
            info->set_keyspace_hits(infoUInt64(values, "keyspace_hits"));
            info->set_keyspace_misses(infoUInt64(values, "keyspace_misses"));
            info->set_expired_keys(infoUInt64(values, "expired_keys"));
            info->set_evicted_keys(infoUInt64(values, "evicted_keys"));
            info->set_rejected_connections(infoUInt64(values, "rejected_connections"));
            info->set_total_error_replies(infoUInt64(values, "total_error_replies"));
            info->set_total_net_input_bytes(infoUInt64(values, "total_net_input_bytes"));
            info->set_total_net_output_bytes(infoUInt64(values, "total_net_output_bytes"));
            info->set_connected_slaves(infoUInt64(values, "connected_slaves"));
            info->set_master_last_io_seconds_ago(infoDouble(values, "master_last_io_seconds_ago"));

            // maxmemory 为 0 表示 Redis 未设置内存上限，此时不计算内存使用百分比。
            if (info->maxmemory() > 0) {
                info->set_memory_used_percent(static_cast<double>(info->used_memory()) /
                                              static_cast<double>(info->maxmemory()) * 100.0);
            }

            const uint64_t keyspaceTotal = info->keyspace_hits() + info->keyspace_misses();
            if (keyspaceTotal > 0) {
                info->set_keyspace_hit_percent(static_cast<double>(info->keyspace_hits()) /
                                               static_cast<double>(keyspaceTotal) * 100.0);
            }

            const std::string role = info->role();
            info->set_replication_configured(role == "master" || role == "slave");
            info->set_master_link_up(role == "master" || infoString(values, "master_link_status") == "up");

            try {
                info->set_slowlog_len(static_cast<uint64_t>(redis.command<long long>("SLOWLOG", "LEN")));
            } catch (const std::exception &) {
                // 慢日志权限或命令不可用时只降级该字段，不影响 INFO 主体指标。
                info->set_slowlog_len(0);
            }
        } catch (const std::exception &ex) {
            info->set_up(false);
            std::cerr << "Redis monitor failed for " << target.instance << ": " << ex.what() << std::endl;
        }
    }
}

} // namespace monitor
