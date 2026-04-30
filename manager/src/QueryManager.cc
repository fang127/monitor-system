#include "QueryManager.h"

#include <mysql.h>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <cstdlib>
#include <ctime>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <iomanip>
#include <climits>
#include <limits>

namespace monitor {

#ifdef ENABLE_MYSQL
constexpr float MAX_SCORE = 100.0f + std::numeric_limits<float>::epsilon();

namespace {

struct SqlParam {
    enum class Type { String, Int64, Double };

    Type type = Type::String;
    std::string string_value;
    long long int_value = 0;
    double double_value = 0.0;
    unsigned long length = 0;
};

SqlParam stringParam(const std::string &value) {
    SqlParam param;
    param.type = SqlParam::Type::String;
    param.string_value = value;
    param.length = static_cast<unsigned long>(param.string_value.size());
    return param;
}

SqlParam intParam(long long value) {
    SqlParam param;
    param.type = SqlParam::Type::Int64;
    param.int_value = value;
    return param;
}

SqlParam doubleParam(double value) {
    SqlParam param;
    param.type = SqlParam::Type::Double;
    param.double_value = value;
    return param;
}

bool bindParams(MYSQL_STMT *stmt, std::vector<MYSQL_BIND> &binds,
                std::vector<SqlParam> &params) {
    if (params.empty()) return true;

    binds.resize(params.size());
    std::memset(binds.data(), 0, sizeof(MYSQL_BIND) * binds.size());

    for (std::size_t i = 0; i < params.size(); ++i) {
        SqlParam &param = params[i];
        switch (param.type) {
        case SqlParam::Type::String:
            binds[i].buffer_type = MYSQL_TYPE_STRING;
            binds[i].buffer =
                const_cast<char *>(param.string_value.data());
            binds[i].buffer_length = param.length;
            binds[i].length = &param.length;
            break;
        case SqlParam::Type::Int64:
            binds[i].buffer_type = MYSQL_TYPE_LONGLONG;
            binds[i].buffer = &param.int_value;
            break;
        case SqlParam::Type::Double:
            binds[i].buffer_type = MYSQL_TYPE_DOUBLE;
            binds[i].buffer = &param.double_value;
            break;
        }
    }

    return mysql_stmt_bind_param(stmt, binds.data()) == 0;
}

bool executePreparedRows(MYSQL *conn, const std::string &sql,
                         std::vector<SqlParam> params,
                         std::vector<std::vector<std::string>> &rows,
                         const char *context) {
    MYSQL_STMT *stmt = mysql_stmt_init(conn);
    if (!stmt) {
        std::cerr << "QueryManager: " << context
                  << " init statement failed: " << mysql_error(conn)
                  << std::endl;
        return false;
    }

    auto closeStmt = [&stmt]() {
        if (stmt) {
            mysql_stmt_close(stmt);
            stmt = nullptr;
        }
    };

    if (mysql_stmt_prepare(stmt, sql.c_str(),
                           static_cast<unsigned long>(sql.size())) != 0) {
        std::cerr << "QueryManager: " << context
                  << " prepare failed: " << mysql_stmt_error(stmt)
                  << std::endl;
        closeStmt();
        return false;
    }

    std::vector<MYSQL_BIND> paramBinds;
    if (!bindParams(stmt, paramBinds, params)) {
        std::cerr << "QueryManager: " << context
                  << " bind params failed: " << mysql_stmt_error(stmt)
                  << std::endl;
        closeStmt();
        return false;
    }

    if (mysql_stmt_execute(stmt) != 0) {
        std::cerr << "QueryManager: " << context
                  << " execute failed: " << mysql_stmt_error(stmt)
                  << std::endl;
        closeStmt();
        return false;
    }

    MYSQL_RES *metadata = mysql_stmt_result_metadata(stmt);
    if (!metadata) {
        closeStmt();
        return true;
    }

    const unsigned int fieldCount = mysql_num_fields(metadata);
    std::vector<MYSQL_BIND> resultBinds(fieldCount);
    std::vector<std::vector<char>> buffers(fieldCount,
                                           std::vector<char>(4096));
    struct BoolFlag {
        bool value = false;
    };
    std::vector<unsigned long> lengths(fieldCount, 0);
    std::vector<BoolFlag> isNull(fieldCount);
    std::vector<BoolFlag> errors(fieldCount);

    std::memset(resultBinds.data(), 0, sizeof(MYSQL_BIND) * fieldCount);
    for (unsigned int i = 0; i < fieldCount; ++i) {
        resultBinds[i].buffer_type = MYSQL_TYPE_STRING;
        resultBinds[i].buffer = buffers[i].data();
        resultBinds[i].buffer_length =
            static_cast<unsigned long>(buffers[i].size());
        resultBinds[i].length = &lengths[i];
        resultBinds[i].is_null = &isNull[i].value;
        resultBinds[i].error = &errors[i].value;
    }

    if (mysql_stmt_bind_result(stmt, resultBinds.data()) != 0 ||
        mysql_stmt_store_result(stmt) != 0) {
        std::cerr << "QueryManager: " << context
                  << " bind result failed: " << mysql_stmt_error(stmt)
                  << std::endl;
        mysql_free_result(metadata);
        closeStmt();
        return false;
    }

    while (true) {
        int status = mysql_stmt_fetch(stmt);
        if (status == MYSQL_NO_DATA) break;
        if (status == 1) {
            std::cerr << "QueryManager: " << context
                      << " fetch failed: " << mysql_stmt_error(stmt)
                      << std::endl;
            mysql_free_result(metadata);
            closeStmt();
            return false;
        }

        std::vector<std::string> row;
        row.reserve(fieldCount);
        for (unsigned int i = 0; i < fieldCount; ++i) {
            if (isNull[i].value) {
                row.emplace_back();
            } else {
                unsigned long len =
                    std::min<unsigned long>(lengths[i], buffers[i].size());
                row.emplace_back(buffers[i].data(), len);
            }
        }
        rows.push_back(std::move(row));
    }

    mysql_free_result(metadata);
    closeStmt();
    return true;
}

int executePreparedCount(MYSQL *conn, const std::string &sql,
                         std::vector<SqlParam> params, const char *context) {
    std::vector<std::vector<std::string>> rows;
    if (!executePreparedRows(conn, sql, std::move(params), rows, context) ||
        rows.empty() || rows[0].empty()) {
        return 0;
    }
    return std::atoi(rows[0][0].c_str());
}

float toFloat(const std::vector<std::string> &row, std::size_t index) {
    return index < row.size() && !row[index].empty()
               ? static_cast<float>(std::atof(row[index].c_str()))
               : 0.0f;
}

uint64_t toU64(const std::vector<std::string> &row, std::size_t index) {
    return index < row.size() && !row[index].empty()
               ? static_cast<uint64_t>(std::stoull(row[index]))
               : 0;
}

int64_t toI64(const std::vector<std::string> &row, std::size_t index) {
    return index < row.size() && !row[index].empty()
               ? static_cast<int64_t>(std::stoll(row[index]))
               : 0;
}

} // namespace
#endif

bool QueryManager::init(const std::string &host, unsigned int port,
                        const std::string &user, const std::string &password,
                        const std::string &db) {
#ifdef ENABLE_MYSQL
    std::lock_guard<std::mutex> lock(mutex_);
    if (initialized_) return true;

    conn_ = mysql_init(nullptr);
    if (!conn_) {
        std::cerr << "MySQL initialization failed" << std::endl;
        return false;
    }

    if (!mysql_real_connect(conn_, host.c_str(), user.c_str(), password.c_str(),
                            db.c_str(), port, nullptr, 0)) {
        std::cerr << "QueryManager mysql_real_connect failed: "
                  << mysql_error(conn_) << std::endl;
        mysql_close(conn_);
        conn_ = nullptr;
        return false;
    }

    // set character set to utf8
    mysql_set_character_set(conn_, "utf8mb4");
    initialized_ = true;
    std::cout << "QueryManager: MySQL connection initialized successfully"
              << std::endl;
    return true;
#else
    (void)host;
    (void)port;
    (void)user;
    (void)password;
    (void)db;
    std::cerr << "QueryManager: MySQL support is not enabled." << std::endl;
    return false;
#endif
}

void QueryManager::close() {
#ifdef ENABLE_MYSQL
    std::lock_guard<std::mutex> lock(mutex_);
    if (conn_) {
        mysql_close(conn_);
        conn_ = nullptr;
        initialized_ = false;
    }
#endif
}

std::string QueryManager::formatTimePoint(
    const std::chrono::system_clock::time_point &tp) const {
    std::time_t time = std::chrono::system_clock::to_time_t(tp);
    char buffer[32];
    std::strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S",
                  std::localtime(&time));
    return std::string(buffer);
}

std::chrono::system_clock::time_point QueryManager::parseTimeString(
    const std::string &timeStr) const {
    std::tm tm = {};
    std::istringstream ss(timeStr);
    ss >> std::get_time(&tm, "%Y-%m-%d %H:%M:%S");
    return std::chrono::system_clock::from_time_t(std::mktime(&tm));
}

bool QueryManager::validateTimeRange(const TimeRange &range) const {
    return range.start_time <= range.end_time;
}

std::vector<PerformanceRecord> QueryManager::queryPerformanceRecords(
    const std::string &serverName, const TimeRange &range, int page,
    int pageSize, int *totalCount) {
    std::vector<PerformanceRecord> records;
#ifdef ENABLE_MYSQL
    std::lock_guard<std::mutex> lock(mutex_);
    // Validate connection and initialization
    if (!initialized_ || !conn_) return records;
    // Validate time range
    if (!validateTimeRange(range)) return records;
    // Validate pagination parameters
    if (page < 1) page = 1;
    // modify pageSize to greeter than 0, otherwise set to default value 100
    if (pageSize <= 0) pageSize = 100;

    std::string startTimeStr = formatTimePoint(range.start_time);
    std::string endTimeStr = formatTimePoint(range.end_time);
    if (totalCount) {
        *totalCount = executePreparedCount(
            conn_,
            "SELECT COUNT(*) FROM server_performance "
            "WHERE server_name=? AND timestamp BETWEEN ? AND ?",
            {stringParam(serverName), stringParam(startTimeStr),
             stringParam(endTimeStr)},
            "performance count");
    }

    // query performance records with pagination
    int offset = (page - 1) * pageSize;
    std::vector<std::vector<std::string>> rows;
    if (!executePreparedRows(
            conn_,
            "SELECT server_name, timestamp, cpu_percent, usr_percent, "
            "system_percent, nice_percent, idle_percent, io_wait_percent, "
            "irq_percent, soft_irq_percent, load_avg_1, load_avg_3, "
            "load_avg_15, mem_used_percent, total, free, avail, "
            "disk_util_percent, send_rate, rcv_rate, score, "
            "cpu_percent_rate, mem_used_percent_rate, "
            "disk_util_percent_rate, load_avg_1_rate, send_rate_rate, "
            "rcv_rate_rate "
            "FROM server_performance WHERE server_name=? "
            "AND timestamp BETWEEN ? AND ? "
            "ORDER BY timestamp DESC LIMIT ? OFFSET ?",
            {stringParam(serverName), stringParam(startTimeStr),
             stringParam(endTimeStr), intParam(pageSize), intParam(offset)},
            rows, "performance query")) {
        return records;
    }

    for (const auto &row : rows) {
        PerformanceRecord rec;
        int i = 0;
        rec.server_name = i < static_cast<int>(row.size()) ? row[i] : "";
        ++i;
        rec.timestamp =
            i < static_cast<int>(row.size()) && !row[i].empty()
                ? parseTimeString(row[i])
                : std::chrono::system_clock::now();
        ++i;
        rec.cpu_percent = toFloat(row, i);
        ++i;
        rec.usr_percent = toFloat(row, i);
        ++i;
        rec.sys_percent = toFloat(row, i);
        ++i;
        rec.nice_percent = toFloat(row, i);
        ++i;
        rec.idle_percent = toFloat(row, i);
        i++;
        rec.io_wait_percent = toFloat(row, i);
        i++;
        rec.irq_percent = toFloat(row, i);
        i++;
        rec.soft_irq_percent = toFloat(row, i);
        i++;
        rec.load_avg_1 = toFloat(row, i);
        i++;
        rec.load_avg_3 = toFloat(row, i);
        i++;
        rec.load_avg_15 = toFloat(row, i);
        i++;
        rec.mem_used_percent = toFloat(row, i);
        i++;
        rec.mem_total = toFloat(row, i);
        i++;
        rec.mem_free = toFloat(row, i);
        i++;
        rec.mem_avail = toFloat(row, i);
        i++;
        rec.disk_util_percent = toFloat(row, i);
        i++;
        rec.net_sent_bytes = toFloat(row, i);
        i++;
        rec.net_recv_bytes = toFloat(row, i);
        i++;
        rec.score = toFloat(row, i);
        i++;
        rec.cpu_percent_rate = toFloat(row, i);
        i++;
        rec.mem_used_percent_rate = toFloat(row, i);
        i++;
        rec.disk_util_percent_rate = toFloat(row, i);
        i++;
        rec.load_avg_1_rate = toFloat(row, i);
        i++;
        rec.net_sent_bytes_rate = toFloat(row, i);
        i++;
        rec.net_recv_bytes_rate = toFloat(row, i);
        records.push_back(rec);
    }
#else
    (void)serverName;
    (void)range;
    (void)page;
    (void)pageSize;
    (void)totalCount;
#endif
    return records;
}

std::vector<PerformanceRecord> QueryManager::queryTrend(
    const std::string &serverName, const TimeRange &range,
    int intervalSeconds) {
    std::vector<PerformanceRecord> records;
#ifdef ENABLE_MYSQL
    std::lock_guard<std::mutex> lock(mutex_);
    // Validate connection and initialization
    if (!initialized_ || !conn_) return records;
    // Validate time range
    if (!validateTimeRange(range)) return records;
    // get start and end time as string
    std::string startTimeStr = formatTimePoint(range.start_time);
    std::string endTimeStr = formatTimePoint(range.end_time);

    std::string query;
    std::vector<SqlParam> params;
    if (intervalSeconds > 0) {
        query =
            "SELECT server_name, "
            "FROM_UNIXTIME(FLOOR(UNIX_TIMESTAMP(timestamp) / ?) * ?) "
            "as time_bucket, "
            "AVG(cpu_percent) as cpu_percent, "
            "AVG(usr_percent) as usr_percent, "
            "AVG(system_percent) as system_percent, "
            "AVG(io_wait_percent) as io_wait_percent, "
            "AVG(load_avg_1) as load_avg_1, "
            "AVG(load_avg_3) as load_avg_3, "
            "AVG(load_avg_15) as load_avg_15, "
            "AVG(mem_used_percent) as mem_used_percent, "
            "AVG(disk_util_percent) as disk_util_percent, "
            "AVG(send_rate) as send_rate, "
            "AVG(rcv_rate) as rcv_rate, "
            "AVG(score) as score, "
            "AVG(cpu_percent_rate) as cpu_percent_rate, "
            "AVG(mem_used_percent_rate) as mem_used_percent_rate, "
            "AVG(disk_util_percent_rate) as disk_util_percent_rate, "
            "AVG(load_avg_1_rate) as load_avg_1_rate "
            "FROM server_performance WHERE server_name=? "
            "AND timestamp BETWEEN ? AND ? "
            "GROUP BY server_name, time_bucket ORDER BY time_bucket";
        params = {intParam(intervalSeconds), intParam(intervalSeconds),
                  stringParam(serverName), stringParam(startTimeStr),
                  stringParam(endTimeStr)};
    } else // no aggregation, just return raw data
    {
        query =
            "SELECT server_name, timestamp, cpu_percent, usr_percent, "
            "system_percent, io_wait_percent, load_avg_1, load_avg_3, "
            "load_avg_15, mem_used_percent, disk_util_percent, send_rate, "
            "rcv_rate, score, cpu_percent_rate, mem_used_percent_rate, "
            "disk_util_percent_rate, load_avg_1_rate "
            "FROM server_performance WHERE server_name=? "
            "AND timestamp BETWEEN ? AND ? ORDER BY timestamp";
        params = {stringParam(serverName), stringParam(startTimeStr),
                  stringParam(endTimeStr)};
    }

    std::vector<std::vector<std::string>> rows;
    if (!executePreparedRows(conn_, query, std::move(params), rows,
                             "trend query")) {
        return records;
    }

    for (const auto &row : rows) {
        PerformanceRecord rec;
        int i = 0;
        rec.server_name = i < static_cast<int>(row.size()) ? row[i] : "";
        ++i;
        rec.timestamp =
            i < static_cast<int>(row.size()) && !row[i].empty()
                ? parseTimeString(row[i])
                : std::chrono::system_clock::now();
        i++;
        rec.cpu_percent = toFloat(row, i);
        i++;
        rec.usr_percent = toFloat(row, i);
        i++;
        rec.sys_percent = toFloat(row, i);
        i++;
        rec.io_wait_percent = toFloat(row, i);
        i++;
        rec.load_avg_1 = toFloat(row, i);
        i++;
        rec.load_avg_3 = toFloat(row, i);
        i++;
        rec.load_avg_15 = toFloat(row, i);
        i++;
        rec.mem_used_percent = toFloat(row, i);
        i++;
        rec.disk_util_percent = toFloat(row, i);
        i++;
        rec.net_sent_bytes_rate = toFloat(row, i);
        i++;
        rec.net_recv_bytes_rate = toFloat(row, i);
        i++;
        rec.score = toFloat(row, i);
        i++;
        rec.cpu_percent_rate = toFloat(row, i);
        i++;
        rec.mem_used_percent_rate = toFloat(row, i);
        i++;
        rec.disk_util_percent_rate = toFloat(row, i);
        i++;
        rec.load_avg_1_rate = toFloat(row, i);
        records.push_back(rec);
    }
#else
    (void)serverName;
    (void)range;
    (void)intervalSeconds;
#endif
    return records;
}

std::vector<AnomalyRecord> QueryManager::queryAnomalyRecords(
    const std::string &serverName, const TimeRange &range,
    const AnomalyThreshold &threshold, int page, int pageSize,
    int *totalCount) {
    std::vector<AnomalyRecord> records;
#ifdef ENABLE_MYSQL
    std::lock_guard<std::mutex> lock(mutex_);
    // Validate connection and initialization
    if (!initialized_ || !conn_) return records;
    // Validate time range
    if (!validateTimeRange(range)) return records;
    // Validate pagination parameters
    if (page < 1) page = 1;
    if (pageSize < 1) pageSize = 100;

    // get start and end time as string
    std::string startTimeStr = formatTimePoint(range.start_time);
    std::string endTimeStr = formatTimePoint(range.end_time);
    std::string whereClause = "timestamp BETWEEN ? AND ?";
    std::vector<SqlParam> whereParams = {stringParam(startTimeStr),
                                         stringParam(endTimeStr)};
    if (!serverName.empty()) {
        whereClause += " AND server_name=?";
        whereParams.push_back(stringParam(serverName));
    }
    whereClause +=
        " AND (cpu_percent > ? OR mem_used_percent > ? "
        "OR disk_util_percent > ? OR ABS(cpu_percent_rate) > ? "
        "OR ABS(mem_used_percent_rate) > ?)";
    whereParams.push_back(doubleParam(threshold.cpu_threshold));
    whereParams.push_back(doubleParam(threshold.memory_threshold));
    whereParams.push_back(doubleParam(threshold.disk_threshold));
    whereParams.push_back(doubleParam(threshold.change_rate_threshold));
    whereParams.push_back(doubleParam(threshold.change_rate_threshold));

    if (totalCount) {
        *totalCount = executePreparedCount(
            conn_,
            "SELECT COUNT(*) FROM server_performance WHERE " + whereClause,
            whereParams, "anomaly count");
    }

    // query anomaly records with pagination
    int offset = (page - 1) * pageSize;
    std::vector<SqlParam> queryParams = whereParams;
    queryParams.push_back(intParam(pageSize));
    queryParams.push_back(intParam(offset));
    std::vector<std::vector<std::string>> rows;
    if (!executePreparedRows(
            conn_,
            "SELECT server_name, timestamp, cpu_percent, mem_used_percent, "
            "disk_util_percent, cpu_percent_rate, mem_used_percent_rate "
            "FROM server_performance WHERE " +
                whereClause + " ORDER BY timestamp DESC LIMIT ? OFFSET ?",
            std::move(queryParams), rows, "anomaly query")) {
        return records;
    }

    for (const auto &row : rows) {
        std::string name = !row.empty() ? row[0] : "";
        auto ts =
            row.size() > 1 && !row[1].empty()
                ? parseTimeString(row[1])
                : std::chrono::system_clock::now();
        float cpu = toFloat(row, 2);
        float mem = toFloat(row, 3);
        float disk = toFloat(row, 4);
        float cpu_rate = toFloat(row, 5);
        float mem_rate = toFloat(row, 6);

        // Check each metric against thresholds and add anomalies
        auto add_anomaly = [&](const std::string &type,
                               const std::string &metric, float value,
                               float threshold) {
            AnomalyRecord rec;
            rec.server_name = name;
            rec.timestamp = ts;
            rec.anomaly_type = type;
            rec.metric_name = metric;
            rec.value = value;
            rec.threshold = threshold;
            // Determine severity based on how much it exceeds the threshold
            if (type == "CPU_HIGH" && value > 95)
                rec.severity = "CRITICAL";
            else if (type == "MEM_HIGH" && value > 95)
                rec.severity = "CRITICAL";
            else if (type == "DISK_HIGH" && value > 95)
                rec.severity = "CRITICAL";
            else if (type == "RATE_SPIKE" && std::abs(value) > 1.0)
                rec.severity = "CRITICAL";
            else
                rec.severity = "WARNING";
            records.push_back(rec);
        };

        if (cpu > threshold.cpu_threshold) {
            add_anomaly("CPU_HIGH", "cpu_percent", cpu,
                        threshold.cpu_threshold);
        }
        if (mem > threshold.memory_threshold) {
            add_anomaly("MEM_HIGH", "mem_used_percent", mem,
                        threshold.memory_threshold);
        }
        if (disk > threshold.disk_threshold) {
            add_anomaly("DISK_HIGH", "disk_util_percent", disk,
                        threshold.disk_threshold);
        }
        if (std::abs(cpu_rate) > threshold.change_rate_threshold) {
            add_anomaly("RATE_SPIKE", "cpu_percent_rate", cpu_rate,
                        threshold.change_rate_threshold);
        }
        if (std::abs(mem_rate) > threshold.change_rate_threshold) {
            add_anomaly("RATE_SPIKE", "mem_used_percent_rate", mem_rate,
                        threshold.change_rate_threshold);
        }
    }
#else
    (void)serverName;
    (void)range;
    (void)threshold;
    (void)page;
    (void)pageSize;
    (void)totalCount;
#endif
    return records;
}

std::vector<ServerScoreSummary> QueryManager::queryServerScoreRank(
    SortOrder order, int page, int pageSize, int *totalCount) {
    std::vector<ServerScoreSummary> records;
#ifdef ENABLE_MYSQL
    std::lock_guard<std::mutex> lock(mutex_);
    // Validate connection and initialization
    if (!initialized_ || !conn_) return records;
    // Validate pagination parameters
    if (page < 1) page = 1;
    if (pageSize < 1) pageSize = 100;

    if (totalCount) {
        *totalCount = executePreparedCount(
            conn_, "SELECT COUNT(DISTINCT server_name) FROM server_performance",
            {}, "score rank count");
    }

    // query total score per server and order by score
    int offset = (page - 1) * pageSize;
    std::string orderBy = (order == SortOrder::ASC) ? "ASC" : "DESC";
    std::string query =
        "SELECT p1.server_name, p1.score, p1.timestamp, p1.cpu_percent, "
        "p1.mem_used_percent, p1.disk_util_percent, p1.load_avg_1 "
        "FROM server_performance p1 "
        "INNER JOIN ("
        "  SELECT server_name, MAX(timestamp) as max_ts "
        "  FROM server_performance GROUP BY server_name"
        ") p2 ON p1.server_name = p2.server_name AND p1.timestamp = "
        "p2.max_ts "
        "ORDER BY p1.score " +
        orderBy + " LIMIT ? OFFSET ?";
    std::vector<std::vector<std::string>> rows;
    if (!executePreparedRows(conn_, query, {intParam(pageSize), intParam(offset)},
                             rows, "score rank query")) {
        return records;
    }

    auto now = std::chrono::system_clock::now();
    for (const auto &row : rows) {
        ServerScoreSummary rec;
        rec.server_name = !row.empty() ? row[0] : "";
        rec.score = toFloat(row, 1);
        rec.last_updated = row.size() > 2 && !row[2].empty()
                               ? parseTimeString(row[2])
                               : now;
        rec.cpu_percent = toFloat(row, 3);
        rec.mem_used_percent = toFloat(row, 4);
        rec.disk_util_percent = toFloat(row, 5);
        rec.load_avg_1 = toFloat(row, 6);

        // Determine server status based on last update time (e.g., if no update
        // for more than 60 seconds, consider it offline)
        auto age = std::chrono::duration_cast<std::chrono::seconds>(
                       now - rec.last_updated)
                       .count();
        rec.status = (age > 60) ? ServerStatus::OFFLINE : ServerStatus::ONLINE;

        records.push_back(rec);
    }
#else
    (void)order;
    (void)page;
    (void)pageSize;
    (void)totalCount;
#endif
    return records;
}

std::vector<ServerScoreSummary> QueryManager::queryLatestServerScores(
    ClusterStats *clusterStats) {
    std::vector<ServerScoreSummary> records;
#ifdef ENABLE_MYSQL
    std::lock_guard<std::mutex> lock(mutex_);
    // Validate connection and initialization
    if (!initialized_ || !conn_) return records;
    // query latest score for each server
    std::string query =
        "SELECT p1.server_name, p1.score, p1.timestamp, p1.cpu_percent, "
        "p1.mem_used_percent, p1.disk_util_percent, p1.load_avg_1 "
        "FROM server_performance p1 "
        "INNER JOIN ("
        "  SELECT server_name, MAX(timestamp) as max_ts "
        "  FROM server_performance GROUP BY server_name"
        ") p2 ON p1.server_name = p2.server_name AND p1.timestamp = p2.max_ts "
        "ORDER BY p1.score DESC";
    std::vector<std::vector<std::string>> rows;
    if (!executePreparedRows(conn_, query, {}, rows, "latest score query")) {
        return records;
    }

    auto now = std::chrono::system_clock::now();
    float totalScore = 0;
    float maxScore = 0;
    float minScore = MAX_SCORE;
    std::string bestServer, worstServer;
    int onlineCount = 0, offlineCount = 0;

    for (const auto &row : rows) {
        ServerScoreSummary rec;
        rec.server_name = !row.empty() ? row[0] : "";
        rec.score = toFloat(row, 1);
        rec.last_updated = row.size() > 2 && !row[2].empty()
                               ? parseTimeString(row[2])
                               : now;
        rec.cpu_percent = toFloat(row, 3);
        rec.mem_used_percent = toFloat(row, 4);
        rec.disk_util_percent = toFloat(row, 5);
        rec.load_avg_1 = toFloat(row, 6);

        // Determine server status based on last update time (e.g., if no update
        // for more than 60 seconds, consider it offline)
        auto age = std::chrono::duration_cast<std::chrono::seconds>(
                       now - rec.last_updated)
                       .count();
        rec.status = (age > 60) ? ServerStatus::OFFLINE : ServerStatus::ONLINE;

        if (rec.status == ServerStatus::ONLINE)
            onlineCount++;
        else
            offlineCount++;

        // Accumulate stats for cluster summary
        totalScore += rec.score;
        if (rec.score > maxScore) {
            maxScore = rec.score;
            bestServer = rec.server_name;
        }
        if (rec.score < minScore) {
            minScore = rec.score;
            worstServer = rec.server_name;
        }

        records.push_back(rec);
    }

    // Fill cluster stats if provided
    if (clusterStats) {
        clusterStats->total_servers = static_cast<int>(records.size());
        clusterStats->online_servers = onlineCount;
        clusterStats->offline_servers = offlineCount;
        clusterStats->avg_score =
            records.empty() ? 0 : totalScore / records.size();
        clusterStats->max_score = maxScore > 0 ? maxScore : 0;
        clusterStats->min_score = minScore < MAX_SCORE ? minScore : 0;
        clusterStats->best_server = bestServer;
        clusterStats->worst_server = worstServer;
    }
#else
    (void)clusterStats;
#endif
    return records;
}

std::vector<NetDetailRecord> QueryManager::queryNetDetailRecords(
    const std::string &serverName, const TimeRange &range, int page,
    int pageSize, int *totalCount) {
    std::vector<NetDetailRecord> records;

#ifdef ENABLE_MYSQL
    std::lock_guard<std::mutex> lock(mutex_);
    if (!initialized_ || !conn_) return records;

    if (!validateTimeRange(range)) return records;
    if (page < 1) page = 1;
    if (pageSize < 1) pageSize = 100;

    std::string startTime = formatTimePoint(range.start_time);
    std::string endTime = formatTimePoint(range.end_time);

    if (totalCount) {
        *totalCount = executePreparedCount(
            conn_,
            "SELECT COUNT(*) FROM server_net_detail "
            "WHERE server_name=? AND timestamp BETWEEN ? AND ?",
            {stringParam(serverName), stringParam(startTime),
             stringParam(endTime)},
            "net detail count");
    }

    // query net detail records with pagination
    int offset = (page - 1) * pageSize;
    std::vector<std::vector<std::string>> rows;
    if (!executePreparedRows(
            conn_,
            "SELECT server_name, net_name, timestamp, err_in, err_out, "
            "drop_in, drop_out, rcv_bytes_rate, snd_bytes_rate, "
            "rcv_packets_rate, snd_packets_rate "
            "FROM server_net_detail WHERE server_name=? "
            "AND timestamp BETWEEN ? AND ? "
            "ORDER BY timestamp DESC LIMIT ? OFFSET ?",
            {stringParam(serverName), stringParam(startTime),
             stringParam(endTime), intParam(pageSize), intParam(offset)},
            rows, "net detail query")) {
        return records;
    }

    for (const auto &row : rows) {
        NetDetailRecord rec;
        int i = 0;
        rec.server_name = i < static_cast<int>(row.size()) ? row[i] : "";
        ++i;
        rec.net_name = i < static_cast<int>(row.size()) ? row[i] : "";
        ++i;
        rec.timestamp =
            i < static_cast<int>(row.size()) && !row[i].empty()
                ? parseTimeString(row[i])
                : std::chrono::system_clock::now();
        i++;
        rec.err_in = toU64(row, i);
        i++;
        rec.err_out = toU64(row, i);
        i++;
        rec.drop_in = toU64(row, i);
        i++;
        rec.drop_out = toU64(row, i);
        i++;
        rec.recv_bytes_rate = toFloat(row, i);
        i++;
        rec.sent_bytes_rate = toFloat(row, i);
        i++;
        rec.recv_packets_rate = toFloat(row, i);
        i++;
        rec.sent_packets_rate = toFloat(row, i);
        records.push_back(rec);
    }
#else
    (void)serverName;
    (void)range;
    (void)page;
    (void)pageSize;
    (void)totalCount;
#endif

    return records;
}

std::vector<DiskDetailRecord> QueryManager::queryDiskDetailRecords(
    const std::string &serverName, const TimeRange &range, int page,
    int pageSize, int *totalCount) {
    std::vector<DiskDetailRecord> records;

#ifdef ENABLE_MYSQL
    std::lock_guard<std::mutex> lock(mutex_);
    if (!initialized_ || !conn_) return records;

    if (!validateTimeRange(range)) return records;
    if (page < 1) page = 1;
    if (pageSize < 1) pageSize = 100;

    std::string startTime = formatTimePoint(range.start_time);
    std::string endTime = formatTimePoint(range.end_time);

    if (totalCount) {
        *totalCount = executePreparedCount(
            conn_,
            "SELECT COUNT(*) FROM server_disk_detail "
            "WHERE server_name=? AND timestamp BETWEEN ? AND ?",
            {stringParam(serverName), stringParam(startTime),
             stringParam(endTime)},
            "disk detail count");
    }

    int offset = (page - 1) * pageSize;
    std::vector<std::vector<std::string>> rows;
    if (!executePreparedRows(
            conn_,
            "SELECT server_name, disk_name, timestamp, read_bytes_per_sec, "
            "write_bytes_per_sec, read_iops, write_iops, avg_read_latency_ms, "
            "avg_write_latency_ms, util_percent "
            "FROM server_disk_detail WHERE server_name=? "
            "AND timestamp BETWEEN ? AND ? "
            "ORDER BY timestamp DESC LIMIT ? OFFSET ?",
            {stringParam(serverName), stringParam(startTime),
             stringParam(endTime), intParam(pageSize), intParam(offset)},
            rows, "disk detail query")) {
        return records;
    }

    for (const auto &row : rows) {
        DiskDetailRecord rec;
        int i = 0;
        rec.server_name = i < static_cast<int>(row.size()) ? row[i] : "";
        ++i;
        rec.disk_name = i < static_cast<int>(row.size()) ? row[i] : "";
        ++i;
        rec.timestamp =
            i < static_cast<int>(row.size()) && !row[i].empty()
                ? parseTimeString(row[i])
                : std::chrono::system_clock::now();
        i++;
        rec.read_bytes_per_sec = toFloat(row, i);
        i++;
        rec.write_bytes_per_sec = toFloat(row, i);
        i++;
        rec.read_iops = toFloat(row, i);
        i++;
        rec.write_iops = toFloat(row, i);
        i++;
        rec.avg_read_latency_ms = toFloat(row, i);
        i++;
        rec.avg_write_latency_ms = toFloat(row, i);
        i++;
        rec.util_percent = toFloat(row, i);
        records.push_back(rec);
    }
#else
    (void)serverName;
    (void)range;
    (void)page;
    (void)pageSize;
    (void)totalCount;
#endif

    return records;
}

std::vector<MemDetailRecord> QueryManager::queryMemDetailRecords(
    const std::string &serverName, const TimeRange &range, int page,
    int pageSize, int *totalCount) {
    std::vector<MemDetailRecord> records;

#ifdef ENABLE_MYSQL
    std::lock_guard<std::mutex> lock(mutex_);
    if (!initialized_ || !conn_) return records;

    if (!validateTimeRange(range)) return records;
    if (page < 1) page = 1;
    if (pageSize < 1) pageSize = 100;

    std::string startTime = formatTimePoint(range.start_time);
    std::string endTime = formatTimePoint(range.end_time);

    if (totalCount) {
        *totalCount = executePreparedCount(
            conn_,
            "SELECT COUNT(*) FROM server_mem_detail "
            "WHERE server_name=? AND timestamp BETWEEN ? AND ?",
            {stringParam(serverName), stringParam(startTime),
             stringParam(endTime)},
            "mem detail count");
    }

    int offset = (page - 1) * pageSize;
    std::vector<std::vector<std::string>> rows;
    if (!executePreparedRows(
            conn_,
            "SELECT server_name, timestamp, total, free, avail, buffers, "
            "cached, active, inactive, dirty "
            "FROM server_mem_detail WHERE server_name=? "
            "AND timestamp BETWEEN ? AND ? "
            "ORDER BY timestamp DESC LIMIT ? OFFSET ?",
            {stringParam(serverName), stringParam(startTime),
             stringParam(endTime), intParam(pageSize), intParam(offset)},
            rows, "mem detail query")) {
        return records;
    }

    for (const auto &row : rows) {
        MemDetailRecord rec;
        int i = 0;
        rec.server_name = i < static_cast<int>(row.size()) ? row[i] : "";
        ++i;
        rec.timestamp =
            i < static_cast<int>(row.size()) && !row[i].empty()
                ? parseTimeString(row[i])
                : std::chrono::system_clock::now();
        i++;
        rec.mem_total = toFloat(row, i);
        i++;
        rec.mem_free = toFloat(row, i);
        i++;
        rec.mem_avail = toFloat(row, i);
        i++;
        rec.buffers = toFloat(row, i);
        i++;
        rec.cached = toFloat(row, i);
        i++;
        rec.active = toFloat(row, i);
        i++;
        rec.inactive = toFloat(row, i);
        i++;
        rec.dirty = toFloat(row, i);
        records.push_back(rec);
    }
#else
    (void)serverName;
    (void)range;
    (void)page;
    (void)pageSize;
    (void)totalCount;
#endif

    return records;
}

std::vector<SoftIrqDetailRecord> QueryManager::querySoftIrqDetailRecords(
    const std::string &serverName, const TimeRange &range, int page,
    int pageSize, int *totalCount) {
    std::vector<SoftIrqDetailRecord> records;

#ifdef ENABLE_MYSQL
    std::lock_guard<std::mutex> lock(mutex_);
    if (!initialized_ || !conn_) return records;

    if (!validateTimeRange(range)) return records;
    if (page < 1) page = 1;
    if (pageSize < 1) pageSize = 100;

    std::string startTime = formatTimePoint(range.start_time);
    std::string endTime = formatTimePoint(range.end_time);

    if (totalCount) {
        *totalCount = executePreparedCount(
            conn_,
            "SELECT COUNT(*) FROM server_softirq_detail "
            "WHERE server_name=? AND timestamp BETWEEN ? AND ?",
            {stringParam(serverName), stringParam(startTime),
             stringParam(endTime)},
            "softirq detail count");
    }

    int offset = (page - 1) * pageSize;
    std::vector<std::vector<std::string>> rows;
    if (!executePreparedRows(
            conn_,
            "SELECT server_name, cpu_name, timestamp, hi, timer, net_tx, "
            "net_rx, block, sched "
            "FROM server_softirq_detail WHERE server_name=? "
            "AND timestamp BETWEEN ? AND ? "
            "ORDER BY timestamp DESC LIMIT ? OFFSET ?",
            {stringParam(serverName), stringParam(startTime),
             stringParam(endTime), intParam(pageSize), intParam(offset)},
            rows, "softirq detail query")) {
        return records;
    }

    for (const auto &row : rows) {
        SoftIrqDetailRecord rec;
        int i = 0;
        rec.server_name = i < static_cast<int>(row.size()) ? row[i] : "";
        ++i;
        rec.cpu_name = i < static_cast<int>(row.size()) ? row[i] : "";
        ++i;
        rec.timestamp =
            i < static_cast<int>(row.size()) && !row[i].empty()
                ? parseTimeString(row[i])
                : std::chrono::system_clock::now();
        i++;
        rec.hi = toI64(row, i);
        i++;
        rec.timer = toI64(row, i);
        i++;
        rec.net_tx = toI64(row, i);
        i++;
        rec.net_rx = toI64(row, i);
        i++;
        rec.block = toI64(row, i);
        i++;
        rec.sched = toI64(row, i);
        records.push_back(rec);
    }
#else
    (void)serverName;
    (void)range;
    (void)page;
    (void)pageSize;
    (void)totalCount;
#endif
    return records;
}

}; // namespace monitor
