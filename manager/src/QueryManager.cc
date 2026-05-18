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

namespace {

/**
 * @brief         Set the Error object
 *
 * @param         error
 * @param         message
 */
void setError(std::string *error, const std::string &message) {
    if (error) *error = message;
}

} // namespace

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

/**
 * @brief         绑定参数到预处理语句
 *
 * @param         stmt
 * @param         binds
 * @param         params
 * @return
 * @return
 */
bool bindParams(MYSQL_STMT *stmt, std::vector<MYSQL_BIND> &binds, std::vector<SqlParam> &params) {
    if (params.empty()) return true;

    binds.resize(params.size());
    std::memset(binds.data(), 0, sizeof(MYSQL_BIND) * binds.size());

    for (std::size_t i = 0; i < params.size(); ++i) {
        SqlParam &param = params[i];
        switch (param.type) {
            case SqlParam::Type::String:
                binds[i].buffer_type = MYSQL_TYPE_STRING;
                binds[i].buffer = const_cast<char *>(param.string_value.data());
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

/**
 * @brief         执行预处理语句并获取结果行
 *
 * @param         conn
 * @param         sql
 * @param         params
 * @param         rows
 * @param         context
 * @return
 * @return
 */
bool executePreparedRows(MYSQL *conn, const std::string &sql, std::vector<SqlParam> params,
                         std::vector<std::vector<std::string>> &rows, const char *context, std::string *error) {
    MYSQL_STMT *stmt = mysql_stmt_init(conn);
    if (!stmt) {
        std::string message = std::string("QueryManager: ") + context + " init statement failed: " + mysql_error(conn);
        std::cerr << message << std::endl;
        setError(error, message);
        return false;
    }

    auto closeStmt = [&stmt]() {
        if (stmt) {
            mysql_stmt_close(stmt);
            stmt = nullptr;
        }
    };

    // 准备预处理语句
    if (mysql_stmt_prepare(stmt, sql.c_str(), static_cast<unsigned long>(sql.size())) != 0) {
        std::string message = std::string("QueryManager: ") + context + " prepare failed: " + mysql_stmt_error(stmt);
        std::cerr << message << std::endl;
        setError(error, message);
        closeStmt();
        return false;
    }

    // 绑定参数
    std::vector<MYSQL_BIND> paramBinds;
    if (!bindParams(stmt, paramBinds, params)) {
        std::string message =
            std::string("QueryManager: ") + context + " bind params failed: " + mysql_stmt_error(stmt);
        std::cerr << message << std::endl;
        setError(error, message);
        closeStmt();
        return false;
    }

    // 执行语句
    if (mysql_stmt_execute(stmt) != 0) {
        std::string message = std::string("QueryManager: ") + context + " execute failed: " + mysql_stmt_error(stmt);
        std::cerr << message << std::endl;
        setError(error, message);
        closeStmt();
        return false;
    }

    // 获取结果元数据以确定字段数量和类型
    MYSQL_RES *metadata = mysql_stmt_result_metadata(stmt);
    if (!metadata) {
        closeStmt();
        return true;
    }

    // 为每个字段准备缓冲区和绑定结构
    const unsigned int fieldCount = mysql_num_fields(metadata);                  // 字段数量
    std::vector<MYSQL_BIND> resultBinds(fieldCount);                             // 结果绑定结构
    std::vector<std::vector<char>> buffers(fieldCount, std::vector<char>(4096)); // 结果缓冲区
    struct BoolFlag {
        bool value = false;
    }; // 用于绑定is_null和error的布尔标志
    std::vector<unsigned long> lengths(fieldCount, 0); // 字段长度
    std::vector<BoolFlag> isNull(fieldCount);          // 字段是否为NULL
    std::vector<BoolFlag> errors(fieldCount);          // 字段是否有错误

    // 初始化绑定结构
    std::memset(resultBinds.data(), 0, sizeof(MYSQL_BIND) * fieldCount);
    for (unsigned int i = 0; i < fieldCount; ++i) {
        resultBinds[i].buffer_type = MYSQL_TYPE_STRING;
        resultBinds[i].buffer = buffers[i].data();
        resultBinds[i].buffer_length = static_cast<unsigned long>(buffers[i].size());
        resultBinds[i].length = &lengths[i];
        resultBinds[i].is_null = &isNull[i].value;
        resultBinds[i].error = &errors[i].value;
    }

    // 绑定结果并获取所有结果行
    if (mysql_stmt_bind_result(stmt, resultBinds.data()) != 0 || mysql_stmt_store_result(stmt) != 0) {
        std::string message =
            std::string("QueryManager: ") + context + " bind result failed: " + mysql_stmt_error(stmt);
        std::cerr << message << std::endl;
        setError(error, message);
        mysql_free_result(metadata);
        closeStmt();
        return false;
    }

    // 循环获取每一行结果，处理NULL值和错误，并将数据转换为字符串存储在rows中
    while (true) {
        int status = mysql_stmt_fetch(stmt);
        if (status == MYSQL_NO_DATA) break;
        if (status == 1) {
            std::string message = std::string("QueryManager: ") + context + " fetch failed: " + mysql_stmt_error(stmt);
            std::cerr << message << std::endl;
            setError(error, message);
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
                unsigned long len = std::min<unsigned long>(lengths[i], buffers[i].size());
                row.emplace_back(buffers[i].data(), len);
            }
        }
        rows.push_back(std::move(row));
    }

    mysql_free_result(metadata);
    closeStmt();
    return true;
}

/**
 * @brief 执行预处理语句并返回第一行第一列的整数结果，适用于COUNT(*)等聚合查询
 *
 * @param         conn
 * @param         sql
 * @param         params
 * @param         context
 * @return
 */
bool executePreparedCount(MYSQL *conn, const std::string &sql, std::vector<SqlParam> params, const char *context,
                          int *count, std::string *error) {
    if (count) *count = 0;

    std::vector<std::vector<std::string>> rows;
    if (!executePreparedRows(conn, sql, std::move(params), rows, context, error)) {
        return false;
    }
    // 如果查询结果不为空且第一行第一列有值，则将其转换为整数并返回
    if (!rows.empty() && !rows[0].empty() && count) *count = std::atoi(rows[0][0].c_str());
    return true;
}

float toFloat(const std::vector<std::string> &row, std::size_t index) {
    return index < row.size() && !row[index].empty() ? static_cast<float>(std::atof(row[index].c_str())) : 0.0f;
}

uint64_t toU64(const std::vector<std::string> &row, std::size_t index) {
    return index < row.size() && !row[index].empty() ? static_cast<uint64_t>(std::stoull(row[index])) : 0;
}

int64_t toI64(const std::vector<std::string> &row, std::size_t index) {
    return index < row.size() && !row[index].empty() ? static_cast<int64_t>(std::stoll(row[index])) : 0;
}

bool toBool(const std::vector<std::string> &row, std::size_t index) {
    if (index >= row.size() || row[index].empty()) return false;
    return row[index] == "1" || row[index] == "true" || row[index] == "TRUE" || row[index] == "yes" ||
           row[index] == "YES";
}

} // namespace
#endif

bool QueryManager::init(MysqlConnectionPool *queryPool, const ManagerConfig *config, ManagerMetrics *metrics) {
#ifdef ENABLE_MYSQL
    std::lock_guard<std::mutex> lock(mutex_);
    if (initialized_) return true;
    if (config) config_ = *config;
    metrics_ = metrics;
    queryPool_ = queryPool;

    if (!queryPool_) {
        std::cerr << "QueryManager: MySQL query pool is required" << std::endl;
        return false;
    }

    initialized_ = true;
    std::cout << "QueryManager: MySQL query pool initialized" << std::endl;
    return true;
#else
    (void)queryPool;
    (void)config;
    (void)metrics;
    std::cerr << "QueryManager: MySQL support is not enabled." << std::endl;
    return false;
#endif
}

void QueryManager::close() {
#ifdef ENABLE_MYSQL
    std::lock_guard<std::mutex> lock(mutex_);
    initialized_ = false;
    queryPool_ = nullptr;
#endif
}

bool QueryManager::isInitialized() const {
#ifdef ENABLE_MYSQL
    return initialized_ && queryPool_ != nullptr;
#else
    return false;
#endif
}

std::string QueryManager::formatTimePoint(const std::chrono::system_clock::time_point &tp) const {
    std::time_t time = std::chrono::system_clock::to_time_t(tp);
    char buffer[32];
    std::strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", std::localtime(&time));
    return std::string(buffer);
}

std::chrono::system_clock::time_point QueryManager::parseTimeString(const std::string &timeStr) const {
    std::tm tm = {};
    std::istringstream ss(timeStr);
    ss >> std::get_time(&tm, "%Y-%m-%d %H:%M:%S");
    return std::chrono::system_clock::from_time_t(std::mktime(&tm));
}

bool QueryManager::validateTimeRange(const TimeRange &range) const { return range.start_time <= range.end_time; }

#ifdef ENABLE_MYSQL
QueryManager::MysqlConnectionLease QueryManager::acquireConnection(std::string *error) {
    MysqlConnectionLease lease;
    if (!initialized_) {
        setError(error, "QueryManager is not initialized");
        return lease;
    }

    if (!queryPool_) {
        setError(error, "QueryManager is not initialized");
        return lease;
    }

    lease.guard = queryPool_->acquire(config_.mysql_read_timeout);
    if (!lease.guard) {
        setError(error, "QueryManager query connection acquire timed out");
        if (metrics_) metrics_->pool_timeouts.fetch_add(1);
        return lease;
    }
    lease.conn = lease.guard.get();
    return lease;
}
#endif

std::vector<PerformanceRecord> QueryManager::queryPerformanceRecords(const std::string &serverName,
                                                                     const TimeRange &range, int page, int pageSize,
                                                                     int *totalCount, std::string *error) {
    std::vector<PerformanceRecord> records;
    if (error) error->clear();
#ifdef ENABLE_MYSQL
    auto lease = acquireConnection(error);
    MYSQL *conn = lease.conn;
    if (!conn) return records;
    // Validate time range
    if (!validateTimeRange(range)) {
        setError(error, "Invalid time range: start_time > end_time");
        return records;
    }
    // Validate pagination parameters
    if (page < 1) page = 1;
    // modify pageSize to greeter than 0, otherwise set to default value 100
    if (pageSize <= 0) pageSize = 100;

    std::string startTimeStr = formatTimePoint(range.start_time);
    std::string endTimeStr = formatTimePoint(range.end_time);
    if (totalCount) {
        // 获取满足条件的记录总数，方便前端分页显示
        if (!executePreparedCount(conn,
                                  "SELECT COUNT(*) FROM server_performance "
                                  "WHERE server_name=? AND timestamp BETWEEN ? AND ?",
                                  {stringParam(serverName), stringParam(startTimeStr), stringParam(endTimeStr)},
                                  "performance count", totalCount, error)) {
            return records;
        }
    }

    // page是从1开始的页码，因此计算offset时需要减1，pageSize是每页记录数
    // query performance records with pagination
    int offset = (page - 1) * pageSize; // 计算分页偏移量
    std::vector<std::vector<std::string>> rows;
    if (!executePreparedRows(conn,
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
                             {stringParam(serverName), stringParam(startTimeStr), stringParam(endTimeStr),
                              intParam(pageSize), intParam(offset)},
                             rows, "performance query", error)) {
        return records;
    }

    for (const auto &row : rows) {
        PerformanceRecord rec;
        int i = 0;
        rec.server_name = i < static_cast<int>(row.size()) ? row[i] : "";
        ++i;
        rec.timestamp = i < static_cast<int>(row.size()) && !row[i].empty() ? parseTimeString(row[i])
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
    setError(error, "QueryManager: MySQL support is not enabled");
    (void)serverName;
    (void)range;
    (void)page;
    (void)pageSize;
    (void)totalCount;
#endif
    return records;
}

std::vector<PerformanceRecord> QueryManager::queryTrend(const std::string &serverName, const TimeRange &range,
                                                        int intervalSeconds, std::string *error) {
    std::vector<PerformanceRecord> records;
    if (error) error->clear();
#ifdef ENABLE_MYSQL
    auto lease = acquireConnection(error);
    MYSQL *conn = lease.conn;
    if (!conn) return records;
    // Validate time range
    if (!validateTimeRange(range)) {
        setError(error, "Invalid time range: start_time > end_time");
        return records;
    }
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
        params = {intParam(intervalSeconds), intParam(intervalSeconds), stringParam(serverName),
                  stringParam(startTimeStr), stringParam(endTimeStr)};
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
        params = {stringParam(serverName), stringParam(startTimeStr), stringParam(endTimeStr)};
    }

    std::vector<std::vector<std::string>> rows;
    if (!executePreparedRows(conn, query, std::move(params), rows, "trend query", error)) {
        return records;
    }

    for (const auto &row : rows) {
        PerformanceRecord rec;
        int i = 0;
        rec.server_name = i < static_cast<int>(row.size()) ? row[i] : "";
        ++i;
        rec.timestamp = i < static_cast<int>(row.size()) && !row[i].empty() ? parseTimeString(row[i])
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
        records.push_back(rec);
    }
#else
    setError(error, "QueryManager: MySQL support is not enabled");
    (void)serverName;
    (void)range;
    (void)intervalSeconds;
#endif
    return records;
}

std::vector<AnomalyRecord> QueryManager::queryAnomalyRecords(const std::string &serverName, const TimeRange &range,
                                                             const AnomalyThreshold &threshold, int page, int pageSize,
                                                             int *totalCount, std::string *error) {
    std::vector<AnomalyRecord> records;
    if (error) error->clear();
#ifdef ENABLE_MYSQL
    auto lease = acquireConnection(error);
    MYSQL *conn = lease.conn;
    if (!conn) return records;
    // Validate time range
    if (!validateTimeRange(range)) {
        setError(error, "Invalid time range: start_time > end_time");
        return records;
    }
    // Validate pagination parameters
    if (page < 1) page = 1;
    if (pageSize < 1) pageSize = 100;

    // get start and end time as string
    std::string startTimeStr = formatTimePoint(range.start_time);
    std::string endTimeStr = formatTimePoint(range.end_time);
    std::string whereClause = "timestamp BETWEEN ? AND ?";
    std::vector<SqlParam> whereParams = {stringParam(startTimeStr), stringParam(endTimeStr)};
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

    std::vector<std::vector<std::string>> rows;
    // 查询满足异常条件的记录，注意这里没有直接在SQL中计算异常类型和严重程度，而是在应用层进行判断和分类，以便更灵活地定义异常规则和级别
    if (!executePreparedRows(conn,
                             "SELECT server_name, timestamp, cpu_percent, mem_used_percent, "
                             "disk_util_percent, cpu_percent_rate, mem_used_percent_rate "
                             "FROM server_performance WHERE " +
                                 whereClause + " ORDER BY timestamp DESC",
                             std::move(whereParams), rows, "anomaly query", error)) {
        return records;
    }

    for (const auto &row : rows) {
        std::string name = !row.empty() ? row[0] : "";
        auto ts = row.size() > 1 && !row[1].empty() ? parseTimeString(row[1]) : std::chrono::system_clock::now();
        float cpu = toFloat(row, 2);
        float mem = toFloat(row, 3);
        float disk = toFloat(row, 4);
        float cpu_rate = toFloat(row, 5);
        float mem_rate = toFloat(row, 6);

        // Check each metric against thresholds and add anomalies
        auto add_anomaly = [&](const std::string &type, const std::string &metric, float value, float threshold) {
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

        // 根据阈值判断是否异常，并记录异常类型、涉及的指标、实际值和阈值等信息，方便前端展示和分析
        if (cpu > threshold.cpu_threshold) {
            add_anomaly("CPU_HIGH", "cpu_percent", cpu, threshold.cpu_threshold);
        }
        if (mem > threshold.memory_threshold) {
            add_anomaly("MEM_HIGH", "mem_used_percent", mem, threshold.memory_threshold);
        }
        if (disk > threshold.disk_threshold) {
            add_anomaly("DISK_HIGH", "disk_util_percent", disk, threshold.disk_threshold);
        }
        if (std::abs(cpu_rate) > threshold.change_rate_threshold) {
            add_anomaly("RATE_SPIKE", "cpu_percent_rate", cpu_rate, threshold.change_rate_threshold);
        }
        if (std::abs(mem_rate) > threshold.change_rate_threshold) {
            add_anomaly("RATE_SPIKE", "mem_used_percent_rate", mem_rate, threshold.change_rate_threshold);
        }
    }

    std::string mysqlWhereClause = "timestamp BETWEEN ? AND ?";
    std::vector<SqlParam> mysqlWhereParams = {stringParam(startTimeStr), stringParam(endTimeStr)};
    if (!serverName.empty()) {
        mysqlWhereClause += " AND server_name=?";
        mysqlWhereParams.push_back(stringParam(serverName));
    }
    mysqlWhereClause +=
        " AND (up=0 OR connection_used_percent > ? "
        "OR (replication_configured=1 AND replication_lag_seconds > ?) "
        "OR slow_queries_rate > ? OR innodb_row_lock_waits_rate > ? "
        "OR (innodb_buffer_pool_hit_percent > 0 AND innodb_buffer_pool_hit_percent < ?))";
    mysqlWhereParams.push_back(doubleParam(threshold.mysql_connection_threshold));
    mysqlWhereParams.push_back(doubleParam(threshold.mysql_replication_lag_threshold));
    mysqlWhereParams.push_back(doubleParam(threshold.mysql_slow_query_rate_threshold));
    mysqlWhereParams.push_back(doubleParam(threshold.mysql_lock_wait_rate_threshold));
    mysqlWhereParams.push_back(doubleParam(threshold.mysql_buffer_pool_hit_threshold));

    std::vector<std::vector<std::string>> mysqlRows;
    if (!executePreparedRows(conn,
                             "SELECT server_name, timestamp, instance, up, connection_used_percent, "
                             "replication_lag_seconds, slow_queries_rate, innodb_row_lock_waits_rate, "
                             "innodb_buffer_pool_hit_percent "
                             "FROM server_mysql_detail WHERE " +
                                 mysqlWhereClause + " ORDER BY timestamp DESC",
                             std::move(mysqlWhereParams), mysqlRows, "mysql anomaly query", error)) {
        return records;
    }

    for (const auto &row : mysqlRows) {
        std::string name = !row.empty() ? row[0] : "";
        auto ts = row.size() > 1 && !row[1].empty() ? parseTimeString(row[1]) : std::chrono::system_clock::now();
        std::string instance = row.size() > 2 ? row[2] : "";
        bool up = toBool(row, 3);
        float connectionUsed = toFloat(row, 4);
        float replicationLag = toFloat(row, 5);
        float slowQueryRate = toFloat(row, 6);
        float lockWaitRate = toFloat(row, 7);
        float bufferPoolHit = toFloat(row, 8);

        auto add_mysql_anomaly = [&](const std::string &type, const std::string &metric, float value,
                                     float thresholdValue, const std::string &severity) {
            AnomalyRecord rec;
            rec.server_name = name;
            rec.timestamp = ts;
            rec.anomaly_type = type;
            rec.severity = severity;
            rec.value = value;
            rec.threshold = thresholdValue;
            rec.metric_name = instance.empty() ? metric : instance + "." + metric;
            records.push_back(rec);
        };

        if (!up) {
            add_mysql_anomaly("MYSQL_DOWN", "up", 0.0f, 1.0f, "CRITICAL");
        }
        if (connectionUsed > threshold.mysql_connection_threshold) {
            add_mysql_anomaly(connectionUsed > 95.0f ? "MYSQL_CONNECTION_CRITICAL" : "MYSQL_CONNECTION_HIGH",
                              "connection_used_percent", connectionUsed, threshold.mysql_connection_threshold,
                              connectionUsed > 95.0f ? "CRITICAL" : "WARNING");
        }
        if (replicationLag > threshold.mysql_replication_lag_threshold) {
            add_mysql_anomaly("MYSQL_REPLICATION_LAG", "replication_lag_seconds", replicationLag,
                              threshold.mysql_replication_lag_threshold,
                              replicationLag > threshold.mysql_replication_lag_threshold * 2 ? "CRITICAL"
                                                                                             : "WARNING");
        }
        if (slowQueryRate > threshold.mysql_slow_query_rate_threshold) {
            add_mysql_anomaly("MYSQL_SLOW_QUERY_SPIKE", "slow_queries_rate", slowQueryRate,
                              threshold.mysql_slow_query_rate_threshold,
                              slowQueryRate > threshold.mysql_slow_query_rate_threshold * 5 ? "CRITICAL"
                                                                                            : "WARNING");
        }
        if (lockWaitRate > threshold.mysql_lock_wait_rate_threshold) {
            add_mysql_anomaly("MYSQL_LOCK_WAIT_SPIKE", "innodb_row_lock_waits_rate", lockWaitRate,
                              threshold.mysql_lock_wait_rate_threshold,
                              lockWaitRate > threshold.mysql_lock_wait_rate_threshold * 5 ? "CRITICAL" : "WARNING");
        }
        if (bufferPoolHit > 0 && bufferPoolHit < threshold.mysql_buffer_pool_hit_threshold) {
            add_mysql_anomaly("MYSQL_BUFFER_POOL_LOW", "innodb_buffer_pool_hit_percent", bufferPoolHit,
                              threshold.mysql_buffer_pool_hit_threshold, bufferPoolHit < 90.0f ? "CRITICAL"
                                                                                               : "WARNING");
        }
    }

    std::sort(records.begin(), records.end(), [](const AnomalyRecord &lhs, const AnomalyRecord &rhs) {
        return lhs.timestamp > rhs.timestamp;
    });

    if (totalCount) *totalCount = static_cast<int>(records.size());
    int offset = (page - 1) * pageSize;
    if (offset >= static_cast<int>(records.size())) {
        records.clear();
    } else {
        auto begin = records.begin() + offset;
        auto end = records.begin() + std::min<int>(offset + pageSize, records.size());
        records = std::vector<AnomalyRecord>(begin, end);
    }
#else
    setError(error, "QueryManager: MySQL support is not enabled");
    (void)serverName;
    (void)range;
    (void)threshold;
    (void)page;
    (void)pageSize;
    (void)totalCount;
#endif
    return records;
}

std::vector<ServerScoreSummary> QueryManager::queryServerScoreRank(SortOrder order, int page, int pageSize,
                                                                   int *totalCount, std::string *error) {
    std::vector<ServerScoreSummary> records;
    if (error) error->clear();
#ifdef ENABLE_MYSQL
    auto lease = acquireConnection(error);
    MYSQL *conn = lease.conn;
    if (!conn) return records;
    // Validate pagination parameters
    if (page < 1) page = 1;
    if (pageSize < 1) pageSize = 100;

    if (totalCount) {
        if (!executePreparedCount(conn, "SELECT COUNT(DISTINCT server_name) FROM server_performance", {},
                                  "score rank count", totalCount, error)) {
            return records;
        }
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
    if (!executePreparedRows(conn, query, {intParam(pageSize), intParam(offset)}, rows, "score rank query", error)) {
        return records;
    }

    auto now = std::chrono::system_clock::now();
    for (const auto &row : rows) {
        ServerScoreSummary rec;
        rec.server_name = !row.empty() ? row[0] : "";
        rec.score = toFloat(row, 1);
        rec.last_updated = row.size() > 2 && !row[2].empty() ? parseTimeString(row[2]) : now;
        rec.cpu_percent = toFloat(row, 3);
        rec.mem_used_percent = toFloat(row, 4);
        rec.disk_util_percent = toFloat(row, 5);
        rec.load_avg_1 = toFloat(row, 6);

        // Determine server status based on last update time (e.g., if no update
        // for more than 60 seconds, consider it offline)
        auto age = std::chrono::duration_cast<std::chrono::seconds>(now - rec.last_updated).count();
        rec.status = (age > 60) ? ServerStatus::OFFLINE : ServerStatus::ONLINE;

        records.push_back(rec);
    }
#else
    setError(error, "QueryManager: MySQL support is not enabled");
    (void)order;
    (void)page;
    (void)pageSize;
    (void)totalCount;
#endif
    return records;
}

std::vector<ServerScoreSummary> QueryManager::queryLatestServerScores(ClusterStats *clusterStats, std::string *error) {
    std::vector<ServerScoreSummary> records;
    if (error) error->clear();
#ifdef ENABLE_MYSQL
    auto lease = acquireConnection(error);
    MYSQL *conn = lease.conn;
    if (!conn) return records;
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
    if (!executePreparedRows(conn, query, {}, rows, "latest score query", error)) return records;

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
        rec.last_updated = row.size() > 2 && !row[2].empty() ? parseTimeString(row[2]) : now;
        rec.cpu_percent = toFloat(row, 3);
        rec.mem_used_percent = toFloat(row, 4);
        rec.disk_util_percent = toFloat(row, 5);
        rec.load_avg_1 = toFloat(row, 6);

        // Determine server status based on last update time (e.g., if no update
        // for more than 60 seconds, consider it offline)
        auto age = std::chrono::duration_cast<std::chrono::seconds>(now - rec.last_updated).count();
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
        clusterStats->avg_score = records.empty() ? 0 : totalScore / records.size();
        clusterStats->max_score = maxScore > 0 ? maxScore : 0;
        clusterStats->min_score = minScore < MAX_SCORE ? minScore : 0;
        clusterStats->best_server = bestServer;
        clusterStats->worst_server = worstServer;
    }
#else
    setError(error, "QueryManager: MySQL support is not enabled");
    (void)clusterStats;
#endif
    return records;
}

std::vector<NetDetailRecord> QueryManager::queryNetDetailRecords(const std::string &serverName, const TimeRange &range,
                                                                 int page, int pageSize, int *totalCount,
                                                                 std::string *error) {
    std::vector<NetDetailRecord> records;
    if (error) error->clear();

#ifdef ENABLE_MYSQL
    auto lease = acquireConnection(error);
    MYSQL *conn = lease.conn;
    if (!conn) return records;

    if (!validateTimeRange(range)) {
        setError(error, "Invalid time range: start_time > end_time");
        return records;
    }
    if (page < 1) page = 1;
    if (pageSize < 1) pageSize = 100;

    std::string startTime = formatTimePoint(range.start_time);
    std::string endTime = formatTimePoint(range.end_time);

    if (totalCount) {
        if (!executePreparedCount(conn,
                                  "SELECT COUNT(*) FROM server_net_detail "
                                  "WHERE server_name=? AND timestamp BETWEEN ? AND ?",
                                  {stringParam(serverName), stringParam(startTime), stringParam(endTime)},
                                  "net detail count", totalCount, error)) {
            return records;
        }
    }

    // query net detail records with pagination
    int offset = (page - 1) * pageSize;
    std::vector<std::vector<std::string>> rows;
    if (!executePreparedRows(conn,
                             "SELECT server_name, net_name, timestamp, err_in, err_out, "
                             "drop_in, drop_out, rcv_bytes_rate, snd_bytes_rate, "
                             "rcv_packets_rate, snd_packets_rate "
                             "FROM server_net_detail WHERE server_name=? "
                             "AND timestamp BETWEEN ? AND ? "
                             "ORDER BY timestamp DESC LIMIT ? OFFSET ?",
                             {stringParam(serverName), stringParam(startTime), stringParam(endTime), intParam(pageSize),
                              intParam(offset)},
                             rows, "net detail query", error)) {
        return records;
    }

    for (const auto &row : rows) {
        NetDetailRecord rec;
        int i = 0;
        rec.server_name = i < static_cast<int>(row.size()) ? row[i] : "";
        ++i;
        rec.net_name = i < static_cast<int>(row.size()) ? row[i] : "";
        ++i;
        rec.timestamp = i < static_cast<int>(row.size()) && !row[i].empty() ? parseTimeString(row[i])
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
    setError(error, "QueryManager: MySQL support is not enabled");
    (void)serverName;
    (void)range;
    (void)page;
    (void)pageSize;
    (void)totalCount;
#endif

    return records;
}

std::vector<DiskDetailRecord> QueryManager::queryDiskDetailRecords(const std::string &serverName,
                                                                   const TimeRange &range, int page, int pageSize,
                                                                   int *totalCount, std::string *error) {
    std::vector<DiskDetailRecord> records;
    if (error) error->clear();

#ifdef ENABLE_MYSQL
    auto lease = acquireConnection(error);
    MYSQL *conn = lease.conn;
    if (!conn) return records;

    if (!validateTimeRange(range)) {
        setError(error, "Invalid time range: start_time > end_time");
        return records;
    }
    if (page < 1) page = 1;
    if (pageSize < 1) pageSize = 100;

    std::string startTime = formatTimePoint(range.start_time);
    std::string endTime = formatTimePoint(range.end_time);

    if (totalCount) {
        if (!executePreparedCount(conn,
                                  "SELECT COUNT(*) FROM server_disk_detail "
                                  "WHERE server_name=? AND timestamp BETWEEN ? AND ?",
                                  {stringParam(serverName), stringParam(startTime), stringParam(endTime)},
                                  "disk detail count", totalCount, error)) {
            return records;
        }
    }

    int offset = (page - 1) * pageSize;
    std::vector<std::vector<std::string>> rows;
    if (!executePreparedRows(conn,
                             "SELECT server_name, disk_name, timestamp, read_bytes_per_sec, "
                             "write_bytes_per_sec, read_iops, write_iops, avg_read_latency_ms, "
                             "avg_write_latency_ms, util_percent "
                             "FROM server_disk_detail WHERE server_name=? "
                             "AND timestamp BETWEEN ? AND ? "
                             "ORDER BY timestamp DESC LIMIT ? OFFSET ?",
                             {stringParam(serverName), stringParam(startTime), stringParam(endTime), intParam(pageSize),
                              intParam(offset)},
                             rows, "disk detail query", error)) {
        return records;
    }

    for (const auto &row : rows) {
        DiskDetailRecord rec;
        int i = 0;
        rec.server_name = i < static_cast<int>(row.size()) ? row[i] : "";
        ++i;
        rec.disk_name = i < static_cast<int>(row.size()) ? row[i] : "";
        ++i;
        rec.timestamp = i < static_cast<int>(row.size()) && !row[i].empty() ? parseTimeString(row[i])
                                                                            : std::chrono::system_clock::now();
        ++i;
        rec.read_bytes_per_sec = toFloat(row, i);
        ++i;
        rec.write_bytes_per_sec = toFloat(row, i);
        ++i;
        rec.read_iops = toFloat(row, i);
        ++i;
        rec.write_iops = toFloat(row, i);
        ++i;
        rec.avg_read_latency_ms = toFloat(row, i);
        ++i;
        rec.avg_write_latency_ms = toFloat(row, i);
        ++i;
        rec.util_percent = toFloat(row, i);
        records.push_back(rec);
    }
#else
    setError(error, "QueryManager: MySQL support is not enabled");
    (void)serverName;
    (void)range;
    (void)page;
    (void)pageSize;
    (void)totalCount;
#endif

    return records;
}

std::vector<MemDetailRecord> QueryManager::queryMemDetailRecords(const std::string &serverName, const TimeRange &range,
                                                                 int page, int pageSize, int *totalCount,
                                                                 std::string *error) {
    std::vector<MemDetailRecord> records;
    if (error) error->clear();

#ifdef ENABLE_MYSQL
    auto lease = acquireConnection(error);
    MYSQL *conn = lease.conn;
    if (!conn) return records;

    if (!validateTimeRange(range)) {
        setError(error, "Invalid time range: start_time > end_time");
        return records;
    }
    if (page < 1) page = 1;
    if (pageSize < 1) pageSize = 100;

    std::string startTime = formatTimePoint(range.start_time);
    std::string endTime = formatTimePoint(range.end_time);

    if (totalCount) {
        if (!executePreparedCount(conn,
                                  "SELECT COUNT(*) FROM server_mem_detail "
                                  "WHERE server_name=? AND timestamp BETWEEN ? AND ?",
                                  {stringParam(serverName), stringParam(startTime), stringParam(endTime)},
                                  "mem detail count", totalCount, error)) {
            return records;
        }
    }

    int offset = (page - 1) * pageSize;
    std::vector<std::vector<std::string>> rows;
    if (!executePreparedRows(conn,
                             "SELECT server_name, timestamp, total, free, avail, buffers, "
                             "cached, active, inactive, dirty "
                             "FROM server_mem_detail WHERE server_name=? "
                             "AND timestamp BETWEEN ? AND ? "
                             "ORDER BY timestamp DESC LIMIT ? OFFSET ?",
                             {stringParam(serverName), stringParam(startTime), stringParam(endTime), intParam(pageSize),
                              intParam(offset)},
                             rows, "mem detail query", error)) {
        return records;
    }

    for (const auto &row : rows) {
        MemDetailRecord rec;
        int i = 0;
        rec.server_name = i < static_cast<int>(row.size()) ? row[i] : "";
        ++i;
        rec.timestamp = i < static_cast<int>(row.size()) && !row[i].empty() ? parseTimeString(row[i])
                                                                            : std::chrono::system_clock::now();
        ++i;
        rec.mem_total = toFloat(row, i);
        ++i;
        rec.mem_free = toFloat(row, i);
        ++i;
        rec.mem_avail = toFloat(row, i);
        ++i;
        rec.buffers = toFloat(row, i);
        ++i;
        rec.cached = toFloat(row, i);
        ++i;
        rec.active = toFloat(row, i);
        ++i;
        rec.inactive = toFloat(row, i);
        ++i;
        rec.dirty = toFloat(row, i);
        records.push_back(rec);
    }
#else
    setError(error, "QueryManager: MySQL support is not enabled");
    (void)serverName;
    (void)range;
    (void)page;
    (void)pageSize;
    (void)totalCount;
#endif

    return records;
}

std::vector<SoftIrqDetailRecord> QueryManager::querySoftIrqDetailRecords(const std::string &serverName,
                                                                         const TimeRange &range, int page, int pageSize,
                                                                         int *totalCount, std::string *error) {
    std::vector<SoftIrqDetailRecord> records;
    if (error) error->clear();

#ifdef ENABLE_MYSQL
    auto lease = acquireConnection(error);
    MYSQL *conn = lease.conn;
    if (!conn) return records;

    if (!validateTimeRange(range)) {
        setError(error, "Invalid time range: start_time > end_time");
        return records;
    }
    if (page < 1) page = 1;
    if (pageSize < 1) pageSize = 100;

    std::string startTime = formatTimePoint(range.start_time);
    std::string endTime = formatTimePoint(range.end_time);

    if (totalCount) {
        if (!executePreparedCount(conn,
                                  "SELECT COUNT(*) FROM server_softirq_detail "
                                  "WHERE server_name=? AND timestamp BETWEEN ? AND ?",
                                  {stringParam(serverName), stringParam(startTime), stringParam(endTime)},
                                  "softirq detail count", totalCount, error)) {
            return records;
        }
    }

    int offset = (page - 1) * pageSize;
    std::vector<std::vector<std::string>> rows;
    if (!executePreparedRows(conn,
                             "SELECT server_name, cpu_name, timestamp, hi, timer, net_tx, "
                             "net_rx, block, sched "
                             "FROM server_softirq_detail WHERE server_name=? "
                             "AND timestamp BETWEEN ? AND ? "
                             "ORDER BY timestamp DESC LIMIT ? OFFSET ?",
                             {stringParam(serverName), stringParam(startTime), stringParam(endTime), intParam(pageSize),
                              intParam(offset)},
                             rows, "softirq detail query", error)) {
        return records;
    }

    for (const auto &row : rows) {
        SoftIrqDetailRecord rec;
        int i = 0;
        rec.server_name = i < static_cast<int>(row.size()) ? row[i] : "";
        ++i;
        rec.cpu_name = i < static_cast<int>(row.size()) ? row[i] : "";
        ++i;
        rec.timestamp = i < static_cast<int>(row.size()) && !row[i].empty() ? parseTimeString(row[i])
                                                                            : std::chrono::system_clock::now();
        ++i;
        rec.hi = toI64(row, i);
        ++i;
        rec.timer = toI64(row, i);
        ++i;
        rec.net_tx = toI64(row, i);
        ++i;
        rec.net_rx = toI64(row, i);
        ++i;
        rec.block = toI64(row, i);
        ++i;
        rec.sched = toI64(row, i);
        records.push_back(rec);
    }
#else
    setError(error, "QueryManager: MySQL support is not enabled");
    (void)serverName;
    (void)range;
    (void)page;
    (void)pageSize;
    (void)totalCount;
#endif
    return records;
}

std::vector<MysqlDetailRecord> QueryManager::queryMysqlDetailRecords(const std::string &serverName,
                                                                     const TimeRange &range, int page, int pageSize,
                                                                     int *totalCount, std::string *error) {
    std::vector<MysqlDetailRecord> records;
    if (error) error->clear();

#ifdef ENABLE_MYSQL
    auto lease = acquireConnection(error);
    MYSQL *conn = lease.conn;
    if (!conn) return records;

    if (!validateTimeRange(range)) {
        setError(error, "Invalid time range: start_time > end_time");
        return records;
    }
    if (page < 1) page = 1;
    if (pageSize < 1) pageSize = 100;

    std::string startTime = formatTimePoint(range.start_time);
    std::string endTime = formatTimePoint(range.end_time);

    if (totalCount) {
        if (!executePreparedCount(conn,
                                  "SELECT COUNT(*) FROM server_mysql_detail "
                                  "WHERE server_name=? AND timestamp BETWEEN ? AND ?",
                                  {stringParam(serverName), stringParam(startTime), stringParam(endTime)},
                                  "mysql detail count", totalCount, error)) {
            return records;
        }
    }

    int offset = (page - 1) * pageSize;
    std::vector<std::vector<std::string>> rows;
    if (!executePreparedRows(conn,
                             "SELECT server_name, instance, timestamp, mysql_host, mysql_port, up, version, `role`, "
                             "max_connections, threads_connected, threads_running, aborted_connects, "
                             "questions, com_select, com_insert, com_update, com_delete, com_commit, com_rollback, "
                             "slow_queries, innodb_buffer_pool_read_requests, innodb_buffer_pool_reads, "
                             "innodb_buffer_pool_hit_percent, innodb_row_lock_waits, innodb_row_lock_time_avg_ms, "
                             "replication_configured, replication_running, replication_lag_seconds, "
                             "connection_used_percent, qps, tps, slow_queries_rate, innodb_row_lock_waits_rate "
                             "FROM server_mysql_detail WHERE server_name=? "
                             "AND timestamp BETWEEN ? AND ? "
                             "ORDER BY timestamp DESC LIMIT ? OFFSET ?",
                             {stringParam(serverName), stringParam(startTime), stringParam(endTime), intParam(pageSize),
                              intParam(offset)},
                             rows, "mysql detail query", error)) {
        return records;
    }

    for (const auto &row : rows) {
        MysqlDetailRecord rec;
        int i = 0;
        rec.server_name = i < static_cast<int>(row.size()) ? row[i] : "";
        ++i;
        rec.instance = i < static_cast<int>(row.size()) ? row[i] : "";
        ++i;
        rec.timestamp = i < static_cast<int>(row.size()) && !row[i].empty() ? parseTimeString(row[i])
                                                                            : std::chrono::system_clock::now();
        ++i;
        rec.mysql_host = i < static_cast<int>(row.size()) ? row[i] : "";
        ++i;
        rec.mysql_port = static_cast<int>(toI64(row, i));
        ++i;
        rec.up = toBool(row, i);
        ++i;
        rec.version = i < static_cast<int>(row.size()) ? row[i] : "";
        ++i;
        rec.role = i < static_cast<int>(row.size()) ? row[i] : "";
        ++i;
        rec.max_connections = toU64(row, i);
        ++i;
        rec.threads_connected = toU64(row, i);
        ++i;
        rec.threads_running = toU64(row, i);
        ++i;
        rec.aborted_connects = toU64(row, i);
        ++i;
        rec.questions = toU64(row, i);
        ++i;
        rec.com_select = toU64(row, i);
        ++i;
        rec.com_insert = toU64(row, i);
        ++i;
        rec.com_update = toU64(row, i);
        ++i;
        rec.com_delete = toU64(row, i);
        ++i;
        rec.com_commit = toU64(row, i);
        ++i;
        rec.com_rollback = toU64(row, i);
        ++i;
        rec.slow_queries = toU64(row, i);
        ++i;
        rec.innodb_buffer_pool_read_requests = toU64(row, i);
        ++i;
        rec.innodb_buffer_pool_reads = toU64(row, i);
        ++i;
        rec.innodb_buffer_pool_hit_percent = toFloat(row, i);
        ++i;
        rec.innodb_row_lock_waits = toU64(row, i);
        ++i;
        rec.innodb_row_lock_time_avg_ms = toFloat(row, i);
        ++i;
        rec.replication_configured = toBool(row, i);
        ++i;
        rec.replication_running = toBool(row, i);
        ++i;
        rec.replication_lag_seconds = toFloat(row, i);
        ++i;
        rec.connection_used_percent = toFloat(row, i);
        ++i;
        rec.qps = toFloat(row, i);
        ++i;
        rec.tps = toFloat(row, i);
        ++i;
        rec.slow_queries_rate = toFloat(row, i);
        ++i;
        rec.innodb_row_lock_waits_rate = toFloat(row, i);
        records.push_back(rec);
    }
#else
    setError(error, "QueryManager: MySQL support is not enabled");
    (void)serverName;
    (void)range;
    (void)page;
    (void)pageSize;
    (void)totalCount;
#endif

    return records;
}

}; // namespace monitor
