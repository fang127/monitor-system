#include "MysqlMonitor.h"

#include <mysql.h>

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

/**
 * @brief         Get the Env object
 *
 * @param         name
 * @param         fallback
 * @return
 */
std::string getEnv(const char *name, const std::string &fallback = "") {
    const char *value = std::getenv(name);
    return value ? std::string(value) : fallback;
}

/**
 * @brief         Is the value truthy (e.g., "1", "true", "yes", "on")?
 *
 * @param         value
 * @return
 * @return
 */
bool isTruthy(const std::string &value) {
    std::string normalized = value;
    std::transform(normalized.begin(), normalized.end(), normalized.begin(),
                   [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
    return normalized == "1" || normalized == "true" || normalized == "yes" || normalized == "on";
}

/**
 * @brief         Parse a port number from a string, with a fallback default value.
 *
 * @param         value
 * @param         fallback
 * @return
 */
uint32_t parsePort(const std::string &value, uint32_t fallback) {
    if (value.empty()) return fallback;
    try {
        int parsed = std::stoi(value);
        return parsed > 0 ? static_cast<uint32_t>(parsed) : fallback;
    } catch (...) {
        return fallback;
    }
}

/**
 * @brief         Parse a timeout value in seconds from a string, with a fallback default value.
 *
 * @param         value
 * @param         fallback
 * @return
 */
unsigned int parseTimeout(const std::string &value, unsigned int fallback) {
    if (value.empty()) return fallback;
    try {
        int parsed = std::stoi(value);
        return parsed > 0 ? static_cast<unsigned int>(parsed) : fallback;
    } catch (...) {
        return fallback;
    }
}

/**
 * @brief         Convert a string to uint64_t, returning 0 on failure or if the string is empty.
 *
 * @param         value
 * @return
 */
uint64_t toUInt64(const std::string &value) {
    if (value.empty()) return 0;
    try {
        return static_cast<uint64_t>(std::stoull(value));
    } catch (...) {
        return 0;
    }
}

/**
 * @brief         Convert a string to double, returning 0.0 on failure or if the string is empty.
 *
 * @param         value
 * @return
 */
double toDouble(const std::string &value) {
    if (value.empty()) return 0.0;
    try {
        return std::stod(value);
    } catch (...) {
        return 0.0;
    }
}

/**
 * @brief         Check if a string value represents an "on" state (e.g., "on", "1", "true", "yes").
 *
 * @param         value
 * @return
 * @return
 */
bool isOn(const std::string &value) {
    std::string normalized = value;
    std::transform(normalized.begin(), normalized.end(), normalized.begin(),
                   [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
    return normalized == "on" || normalized == "1" || normalized == "true" || normalized == "yes";
}

/**
 * @brief         Split a string by a delimiter into a vector of strings.
 *
 * @param         value
 * @param         delimiter
 * @return
 */
std::vector<std::string> split(const std::string &value, char delimiter) {
    std::vector<std::string> parts;
    std::string item;
    std::istringstream iss(value);
    while (std::getline(iss, item, delimiter)) parts.push_back(item);
    return parts;
}

/**
 * @brief         查询MySQL服务器的变量或状态，返回一个键值对映射。查询结果必须包含两列，第一列作为键，第二列作为值。
 *
 * @param         conn
 * @param         sql
 * @return
 */
std::map<std::string, std::string> queryNameValue(MYSQL *conn, const std::string &sql) {
    std::map<std::string, std::string> values;
    if (mysql_query(conn, sql.c_str()) != 0) return values;

    MYSQL_RES *result = mysql_store_result(conn);
    if (!result) return values;

    MYSQL_ROW row;
    while ((row = mysql_fetch_row(result)) != nullptr) {
        if (row[0] && row[1]) values[row[0]] = row[1];
    }

    mysql_free_result(result);
    return values;
}

/**
 * @brief         查询MySQL复制状态，返回一个包含复制配置、运行状态和延迟信息的结构体。
 *
 */
struct ReplicationStatus {
    bool configured = false;
    bool running = false;
    double lagSeconds = 0.0;
};

/**
 * @brief         查询MySQL复制状态，支持SHOW REPLICA STATUS和SHOW SLAVE STATUS两种语法，适用于不同版本的MySQL服务器。
 *
 * @param         conn
 * @return
 */
ReplicationStatus queryReplicationStatus(MYSQL *conn) {
    auto parseResult = [](MYSQL_RES *result) {
        ReplicationStatus status;
        if (!result) return status;

        MYSQL_ROW row = mysql_fetch_row(result);
        if (!row) {
            mysql_free_result(result);
            return status;
        }

        status.configured = true;
        MYSQL_FIELD *fields = mysql_fetch_fields(result);
        unsigned int fieldCount = mysql_num_fields(result);
        std::map<std::string, std::string> values;
        for (unsigned int i = 0; i < fieldCount; ++i) {
            if (fields[i].name && row[i]) values[fields[i].name] = row[i];
        }

        std::string ioRunning =
            values.count("Replica_IO_Running") ? values["Replica_IO_Running"] : values["Slave_IO_Running"];
        std::string sqlRunning =
            values.count("Replica_SQL_Running") ? values["Replica_SQL_Running"] : values["Slave_SQL_Running"];
        status.running = ioRunning == "Yes" && sqlRunning == "Yes";
        if (values.count("Seconds_Behind_Source"))
            status.lagSeconds = toDouble(values["Seconds_Behind_Source"]);
        else if (values.count("Seconds_Behind_Master"))
            status.lagSeconds = toDouble(values["Seconds_Behind_Master"]);

        mysql_free_result(result);
        return status;
    };

    if (mysql_query(conn, "SHOW REPLICA STATUS") == 0) {
        if (MYSQL_RES *result = mysql_store_result(conn)) return parseResult(result);
    }

    if (mysql_query(conn, "SHOW SLAVE STATUS") == 0) {
        if (MYSQL_RES *result = mysql_store_result(conn)) return parseResult(result);
    }

    return {};
}

/**
 * @brief
 * 根据MySQL服务器的变量信息和可选的角色提示，推断服务器的角色（primary或replica）。如果read_only或super_read_only变量为ON，则认为是replica，否则默认为primary。角色提示优先于变量信息，但如果提示为"unknown"或为空，则会根据变量进行推断。
 *
 * @param         variables
 * @param         roleHint
 * @return
 */
std::string inferRole(const std::map<std::string, std::string> &variables, const std::string &roleHint) {
    if (!roleHint.empty() && roleHint != "unknown") return roleHint;

    auto readOnly = variables.find("read_only");
    auto superReadOnly = variables.find("super_read_only");
    if ((readOnly != variables.end() && isOn(readOnly->second)) ||
        (superReadOnly != variables.end() && isOn(superReadOnly->second))) {
        return "replica";
    }
    return "primary";
}

} // namespace

MysqlMonitor::MysqlMonitor() {
    const std::string targetConfig = getEnv("MYSQL_MONITOR_TARGETS");
    enabled_ = isTruthy(getEnv("MYSQL_MONITOR_ENABLED")) || !targetConfig.empty();
    timeoutSeconds_ = parseTimeout(getEnv("MYSQL_MONITOR_TIMEOUT_SECONDS"), 3);

    if (!targetConfig.empty()) {
        for (const std::string &entry : split(targetConfig, ';')) {
            if (entry.empty()) continue;
            std::vector<std::string> parts = split(entry, '|');
            Target target;
            if (parts.size() > 0) target.instance = parts[0];
            if (parts.size() > 1 && !parts[1].empty()) target.host = parts[1];
            if (parts.size() > 2) target.port = parsePort(parts[2], target.port);
            if (parts.size() > 3) target.user = parts[3];
            if (parts.size() > 4) target.password = parts[4];
            if (parts.size() > 5 && !parts[5].empty()) target.role_hint = parts[5];
            if (target.instance.empty()) target.instance = target.host + ":" + std::to_string(target.port);
            targets_.push_back(target);
        }
    } else if (enabled_) {
        Target target;
        target.host = getEnv("MYSQL_MONITOR_HOST", target.host);
        target.port = parsePort(getEnv("MYSQL_MONITOR_PORT"), target.port);
        target.user = getEnv("MYSQL_MONITOR_USER");
        target.password = getEnv("MYSQL_MONITOR_PASSWORD");
        target.instance = getEnv("MYSQL_MONITOR_INSTANCE", target.host + ":" + std::to_string(target.port));
        target.role_hint = getEnv("MYSQL_MONITOR_ROLE", target.role_hint);
        targets_.push_back(target);
    }

    if (enabled_) {
        std::cout << "MysqlMonitor enabled, targets: " << targets_.size() << std::endl;
    }
}

void MysqlMonitor::updateOnce(monitor::proto::MonitorInfo *monitorInfo) {
    if (!enabled_ || !monitorInfo) return;

    for (const Target &target : targets_) {
        auto *info = monitorInfo->add_mysql_info();
        info->set_instance(target.instance);
        info->set_host(target.host);
        info->set_port(target.port);
        info->set_role(target.role_hint.empty() ? "unknown" : target.role_hint);

        MYSQL *conn = mysql_init(nullptr);
        if (!conn) {
            info->set_up(false);
            continue;
        }

        mysql_options(conn, MYSQL_OPT_CONNECT_TIMEOUT, &timeoutSeconds_);
        mysql_options(conn, MYSQL_OPT_READ_TIMEOUT, &timeoutSeconds_);
        mysql_options(conn, MYSQL_OPT_WRITE_TIMEOUT, &timeoutSeconds_);

        MYSQL *connected = mysql_real_connect(conn, target.host.c_str(), target.user.c_str(), target.password.c_str(),
                                              nullptr, target.port, nullptr, 0);
        if (!connected) {
            info->set_up(false);
            mysql_close(conn);
            continue;
        }

        info->set_up(true);
        if (const char *version = mysql_get_server_info(conn)) info->set_version(version);

        const auto variables = queryNameValue(conn,
                                              "SHOW GLOBAL VARIABLES WHERE Variable_name IN "
                                              "('max_connections','read_only','super_read_only')");
        const auto status =
            queryNameValue(conn,
                           "SHOW GLOBAL STATUS WHERE Variable_name IN "
                           "('Threads_connected','Threads_running','Aborted_connects','Questions',"
                           "'Com_select','Com_insert','Com_update','Com_delete','Com_commit','Com_rollback',"
                           "'Slow_queries','Innodb_buffer_pool_read_requests','Innodb_buffer_pool_reads',"
                           "'Innodb_row_lock_waits','Innodb_row_lock_time_avg')");

        info->set_role(inferRole(variables, target.role_hint));
        if (variables.count("max_connections")) info->set_max_connections(toUInt64(variables.at("max_connections")));

        if (status.count("Threads_connected")) info->set_threads_connected(toUInt64(status.at("Threads_connected")));
        if (status.count("Threads_running")) info->set_threads_running(toUInt64(status.at("Threads_running")));
        if (status.count("Aborted_connects")) info->set_aborted_connects(toUInt64(status.at("Aborted_connects")));
        if (status.count("Questions")) info->set_questions(toUInt64(status.at("Questions")));
        if (status.count("Com_select")) info->set_com_select(toUInt64(status.at("Com_select")));
        if (status.count("Com_insert")) info->set_com_insert(toUInt64(status.at("Com_insert")));
        if (status.count("Com_update")) info->set_com_update(toUInt64(status.at("Com_update")));
        if (status.count("Com_delete")) info->set_com_delete(toUInt64(status.at("Com_delete")));
        if (status.count("Com_commit")) info->set_com_commit(toUInt64(status.at("Com_commit")));
        if (status.count("Com_rollback")) info->set_com_rollback(toUInt64(status.at("Com_rollback")));
        if (status.count("Slow_queries")) info->set_slow_queries(toUInt64(status.at("Slow_queries")));
        if (status.count("Innodb_buffer_pool_read_requests")) {
            info->set_innodb_buffer_pool_read_requests(toUInt64(status.at("Innodb_buffer_pool_read_requests")));
        }
        if (status.count("Innodb_buffer_pool_reads")) {
            info->set_innodb_buffer_pool_reads(toUInt64(status.at("Innodb_buffer_pool_reads")));
        }
        if (status.count("Innodb_row_lock_waits")) {
            info->set_innodb_row_lock_waits(toUInt64(status.at("Innodb_row_lock_waits")));
        }
        if (status.count("Innodb_row_lock_time_avg")) {
            info->set_innodb_row_lock_time_avg_ms(toDouble(status.at("Innodb_row_lock_time_avg")));
        }

        if (info->innodb_buffer_pool_read_requests() > 0) {
            double hitPercent = (1.0 - static_cast<double>(info->innodb_buffer_pool_reads()) /
                                           static_cast<double>(info->innodb_buffer_pool_read_requests())) *
                                100.0;
            info->set_innodb_buffer_pool_hit_percent(hitPercent < 0.0 ? 0.0 : hitPercent);
        }

        ReplicationStatus replication = queryReplicationStatus(conn);
        info->set_replication_configured(replication.configured);
        info->set_replication_running(replication.running);
        info->set_replication_lag_seconds(replication.lagSeconds);

        mysql_close(conn);
    }
}

} // namespace monitor
