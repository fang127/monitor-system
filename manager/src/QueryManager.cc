#include "QueryManager.h"

#include <mysql.h>
#include <cstdlib>
#include <ctime>
#include <iostream>
#include <mutex>
#include <sstream>
#include <string>
#include <vector>
#include <iomanip>
#include <climits>

namespace monitor
{
constexpr float MAX_SCORE = 100.0f + std::numeric_limits<float>::epsilon();

bool QueryManager::init(const std::string &host, const std::string &user,
                        const std::string &password, const std::string &db)
{
#ifdef ENABLE_MYSQL
    std::lock_guard<std::mutex> lock(mutex_);
    if (initialized_) return true;

    conn_ = mysql_init(nullptr);
    if (!conn_)
    {
        std::cerr << "MySQL initialization failed" << std::endl;
        return false;
    }

    if (!mysql_real_connect(conn_, host.c_str(), user.c_str(), password.c_str(),
                            db.c_str(), 0, nullptr, 0))
    {
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
    (void)user;
    (void)password;
    (void)db;
    std::cerr << "QueryManager: MySQL support is not enabled." << std::endl;
    return false;
#endif
}

void QueryManager::close()
{
#ifdef ENABLE_MYSQL
    std::lock_guard<std::mutex> lock(mutex_);
    if (conn_)
    {
        mysql_close(conn_);
        conn_ = nullptr;
        initialized_ = false;
    }
#endif
}

std::string QueryManager::formatTimePoint(
    const std::chrono::system_clock::time_point &tp) const
{
    std::time_t time = std::chrono::system_clock::to_time_t(tp);
    char buffer[32];
    std::strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S",
                  std::localtime(&time));
    return std::string(buffer);
}

std::chrono::system_clock::time_point QueryManager::parseTimeString(
    const std::string &timeStr) const
{
    std::tm tm = {};
    std::istringstream ss(timeStr);
    ss >> std::get_time(&tm, "%Y-%m-%d %H:%M:%S");
    return std::chrono::system_clock::from_time_t(std::mktime(&tm));
}

int QueryManager::getTotalCount(const std::string &countQuery)
{
#ifdef ENABLE_MYSQL
    if (mysql_query(conn_, countQuery.c_str()) != 0)
    {
        std::cerr << "QueryManager: count query failed: " << mysql_error(conn_)
                  << std::endl;
        return 0;
    }

    MYSQL_RES *res = mysql_store_result(conn_);
    if (!res) return 0;

    int totalCount = 0;
    MYSQL_ROW row = mysql_fetch_row(res);
    if (row && row[0]) totalCount = std::atoi(row[0]);
    mysql_free_result(res);
    return totalCount;
#else
    (void)countQuery;
    return 0;
#endif
}

bool QueryManager::validateTimeRange(const TimeRange &range) const
{
    return range.start_time <= range.end_time;
}

std::vector<PerformanceRecord> QueryManager::queryPerformanceRecords(
    const std::string &serverName, const TimeRange &range, int page,
    int pageSize, int *totalCount)
{
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
    // get total count of records
    std::ostringstream countQuery;
    countQuery << "SELECT COUNT(*) FROM server_performance WHERE server_name='"
               << serverName << "' AND timestamp BETWEEN '" << startTimeStr
               << "' AND '" << endTimeStr << "'";
    if (totalCount) *totalCount = getTotalCount(countQuery.str());

    // query performance records with pagination
    int offset = (page - 1) * pageSize;
    std::ostringstream query;
    query << "SELECT server_name, timestamp, cpu_percent, usr_percent, "
             "system_percent, nice_percent, idle_percent, io_wait_percent, "
             "irq_percent, soft_irq_percent, load_avg_1, load_avg_3, "
             "load_avg_15, "
             "mem_used_percent, total, free, avail, disk_util_percent, "
             "send_rate, rcv_rate, score, cpu_percent_rate, "
             "mem_used_percent_rate, "
             "disk_util_percent_rate, load_avg_1_rate, send_rate_rate, "
             "rcv_rate_rate "
             "FROM server_performance WHERE server_name='"
          << serverName << "' AND timestamp BETWEEN '" << startTimeStr
          << "' AND '" << endTimeStr << "' ORDER BY timestamp DESC LIMIT "
          << pageSize << " OFFSET " << offset;
    // execute query
    if (mysql_query(conn_, query.str().c_str()) != 0)
    {
        std::cerr << "QueryManager: query failed: " << mysql_error(conn_)
                  << std::endl;
        return records;
    }
    // fetch results
    MYSQL_RES *res = mysql_store_result(conn_);
    if (!res) return records;
    MYSQL_ROW row;
    while ((row = mysql_fetch_row(res)))
    {
        PerformanceRecord rec;
        int i = 0;
        rec.server_name = row[i++] ? row[i - 1] : "";
        rec.timestamp =
            row[i] ? parseTimeString(row[i]) : std::chrono::system_clock::now();
        ++i;
        rec.cpu_percent = row[i] ? std::atof(row[i]) : 0.0;
        ++i;
        rec.usr_percent = row[i] ? std::atof(row[i]) : 0.0;
        ++i;
        rec.sys_percent = row[i] ? std::atof(row[i]) : 0.0;
        ++i;
        rec.nice_percent = row[i] ? std::atof(row[i]) : 0.0;
        ++i;
        rec.idle_percent = row[i] ? std::atof(row[i]) : 0;
        i++;
        rec.io_wait_percent = row[i] ? std::atof(row[i]) : 0;
        i++;
        rec.irq_percent = row[i] ? std::atof(row[i]) : 0;
        i++;
        rec.soft_irq_percent = row[i] ? std::atof(row[i]) : 0;
        i++;
        rec.load_avg_1 = row[i] ? std::atof(row[i]) : 0;
        i++;
        rec.load_avg_3 = row[i] ? std::atof(row[i]) : 0;
        i++;
        rec.load_avg_15 = row[i] ? std::atof(row[i]) : 0;
        i++;
        rec.mem_used_percent = row[i] ? std::atof(row[i]) : 0;
        i++;
        rec.mem_total = row[i] ? std::atof(row[i]) : 0;
        i++;
        rec.mem_free = row[i] ? std::atof(row[i]) : 0;
        i++;
        rec.mem_avail = row[i] ? std::atof(row[i]) : 0;
        i++;
        rec.disk_util_percent = row[i] ? std::atof(row[i]) : 0;
        i++;
        rec.net_sent_bytes = row[i] ? std::atof(row[i]) : 0;
        i++;
        rec.net_recv_bytes = row[i] ? std::atof(row[i]) : 0;
        i++;
        rec.score = row[i] ? std::atof(row[i]) : 0;
        i++;
        rec.cpu_percent_rate = row[i] ? std::atof(row[i]) : 0;
        i++;
        rec.mem_used_percent_rate = row[i] ? std::atof(row[i]) : 0;
        i++;
        rec.disk_util_percent_rate = row[i] ? std::atof(row[i]) : 0;
        i++;
        rec.load_avg_1_rate = row[i] ? std::atof(row[i]) : 0;
        i++;
        rec.net_sent_bytes_rate = row[i] ? std::atof(row[i]) : 0;
        i++;
        rec.net_recv_bytes_rate = row[i] ? std::atof(row[i]) : 0;
        records.push_back(rec);
    }
    mysql_free_result(res);
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
    const std::string &serverName, const TimeRange &range, int intervalSeconds)
{
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

    // ready command
    std::ostringstream query;
    if (intervalSeconds > 0)
    {
        query << "SELECT server_name, "
                 "FROM_UNIXTIME(FLOOR(UNIX_TIMESTAMP(timestamp) / "
              << intervalSeconds << ") * " << intervalSeconds
              << ") as time_bucket, "
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
                 "FROM server_performance WHERE server_name='"
              << serverName << "' AND timestamp BETWEEN '" << startTimeStr
              << "' AND '" << endTimeStr
              << "' GROUP BY server_name, time_bucket ORDER BY time_bucket";
    }
    else // no aggregation, just return raw data
    {
        query << "SELECT server_name, timestamp, cpu_percent, usr_percent, "
                 "system_percent, io_wait_percent, load_avg_1, load_avg_3, "
                 "load_avg_15, mem_used_percent, disk_util_percent, send_rate, "
                 "rcv_rate, score, cpu_percent_rate, mem_used_percent_rate, "
                 "disk_util_percent_rate, load_avg_1_rate "
                 "FROM server_performance WHERE server_name='"
              << serverName << "' AND timestamp BETWEEN '" << startTimeStr
              << "' AND '" << endTimeStr << "' ORDER BY timestamp";
    }

    if (mysql_query(conn_, query.str().c_str()) != 0)
    {
        std::cerr << "QueryManager: trend query failed: " << mysql_error(conn_)
                  << std::endl;
        return records;
    }

    MYSQL_RES *res = mysql_store_result(conn_);
    if (!res) return records;
    MYSQL_ROW row;
    while ((row = mysql_fetch_row(res)))
    {
        PerformanceRecord rec;
        int i = 0;
        rec.server_name = row[i++] ? row[i - 1] : "";
        rec.timestamp =
            row[i] ? parseTimeString(row[i]) : std::chrono::system_clock::now();
        i++;
        rec.cpu_percent = row[i] ? std::atof(row[i]) : 0;
        i++;
        rec.usr_percent = row[i] ? std::atof(row[i]) : 0;
        i++;
        rec.sys_percent = row[i] ? std::atof(row[i]) : 0;
        i++;
        rec.io_wait_percent = row[i] ? std::atof(row[i]) : 0;
        i++;
        rec.load_avg_1 = row[i] ? std::atof(row[i]) : 0;
        i++;
        rec.load_avg_3 = row[i] ? std::atof(row[i]) : 0;
        i++;
        rec.load_avg_15 = row[i] ? std::atof(row[i]) : 0;
        i++;
        rec.mem_used_percent = row[i] ? std::atof(row[i]) : 0;
        i++;
        rec.disk_util_percent = row[i] ? std::atof(row[i]) : 0;
        i++;
        rec.net_sent_bytes_rate = row[i] ? std::atof(row[i]) : 0;
        i++;
        rec.net_recv_bytes_rate = row[i] ? std::atof(row[i]) : 0;
        i++;
        rec.score = row[i] ? std::atof(row[i]) : 0;
        i++;
        rec.cpu_percent_rate = row[i] ? std::atof(row[i]) : 0;
        i++;
        rec.mem_used_percent_rate = row[i] ? std::atof(row[i]) : 0;
        i++;
        rec.disk_util_percent_rate = row[i] ? std::atof(row[i]) : 0;
        i++;
        rec.load_avg_1_rate = row[i] ? std::atof(row[i]) : 0;
        records.push_back(rec);
    }
    mysql_free_result(res);
#else
    (void)serverName;
    (void)range;
    (void)intervalSeconds;
#endif
    return records;
}

std::vector<AnomalyRecord> QueryManager::queryAnomalyRecords(
    const std::string &serverName, const TimeRange &range,
    const AnomalyThreshold &threshold, int page, int pageSize, int *totalCount)
{
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
    // build where clause for anomaly conditions
    std::ostringstream whereClause;
    whereClause << "timestamp BETWEEN '" << startTimeStr << "' AND '"
                << endTimeStr << "'";
    if (!serverName.empty())
        whereClause << " AND server_name='" << serverName << "'";
    whereClause << " AND (cpu_percent > " << threshold.cpu_threshold
                << " OR mem_used_percent > " << threshold.memory_threshold
                << " OR disk_util_percent > " << threshold.disk_threshold
                << " OR ABS(cpu_percent_rate) > "
                << threshold.change_rate_threshold
                << " OR ABS(mem_used_percent_rate) > "
                << threshold.change_rate_threshold << ")";
    // get total count of records
    std::ostringstream countQuery;
    countQuery << "SELECT COUNT(*) FROM server_performance WHERE "
               << whereClause.str();
    if (totalCount) *totalCount = getTotalCount(countQuery.str());

    // query anomaly records with pagination
    int offset = (page - 1) * pageSize;
    std::ostringstream query;
    query << "SELECT server_name, timestamp, cpu_percent, mem_used_percent, "
             "disk_util_percent, cpu_percent_rate, mem_used_percent_rate "
             "FROM server_performance WHERE "
          << whereClause.str() << " ORDER BY timestamp DESC LIMIT " << pageSize
          << " OFFSET " << offset;
    if (mysql_query(conn_, query.str().c_str()))
    {
        std::cerr << "QueryManager: anomaly query failed: "
                  << mysql_error(conn_) << std::endl;
        return records;
    }

    // fetch results
    MYSQL_RES *res = mysql_store_result(conn_);
    if (!res) return records;
    MYSQL_ROW row;
    while ((row = mysql_fetch_row(res)))
    {
        std::string name = row[0] ? row[0] : "";
        auto ts =
            row[1] ? parseTimeString(row[1]) : std::chrono::system_clock::now();
        float cpu = row[2] ? std::atof(row[2]) : 0;
        float mem = row[3] ? std::atof(row[3]) : 0;
        float disk = row[4] ? std::atof(row[4]) : 0;
        float cpu_rate = row[5] ? std::atof(row[5]) : 0;
        float mem_rate = row[6] ? std::atof(row[6]) : 0;

        // Check each metric against thresholds and add anomalies
        auto add_anomaly = [&](const std::string &type,
                               const std::string &metric, float value,
                               float threshold)
        {
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

        if (cpu > threshold.cpu_threshold)
        {
            add_anomaly("CPU_HIGH", "cpu_percent", cpu,
                        threshold.cpu_threshold);
        }
        if (mem > threshold.memory_threshold)
        {
            add_anomaly("MEM_HIGH", "mem_used_percent", mem,
                        threshold.memory_threshold);
        }
        if (disk > threshold.disk_threshold)
        {
            add_anomaly("DISK_HIGH", "disk_util_percent", disk,
                        threshold.disk_threshold);
        }
        if (std::abs(cpu_rate) > threshold.change_rate_threshold)
        {
            add_anomaly("RATE_SPIKE", "cpu_percent_rate", cpu_rate,
                        threshold.change_rate_threshold);
        }
        if (std::abs(mem_rate) > threshold.change_rate_threshold)
        {
            add_anomaly("RATE_SPIKE", "mem_used_percent_rate", mem_rate,
                        threshold.change_rate_threshold);
        }
    }
    mysql_free_result(res);
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
    SortOrder order, int page, int pageSize, int *totalCount)
{
    std::vector<ServerScoreSummary> records;
#ifdef ENABLE_MYSQL
    std::lock_guard<std::mutex> lock(mutex_);
    // Validate connection and initialization
    if (!initialized_ || !conn_) return records;
    // Validate pagination parameters
    if (page < 1) page = 1;
    if (pageSize < 1) pageSize = 100;

    // get total count of servers
    std::string countQuery =
        "SELECT COUNT(DISTINCT server_name) FROM server_performance";
    if (totalCount) *totalCount = getTotalCount(countQuery);

    // query total score per server and order by score
    int offset = (page - 1) * pageSize;
    std::string orderBy = (order == SortOrder::ASC) ? "ASC" : "DESC";
    std::ostringstream query;
    query << "SELECT p1.server_name, p1.score, p1.timestamp, p1.cpu_percent, "
             "p1.mem_used_percent, p1.disk_util_percent, p1.load_avg_1 "
             "FROM server_performance p1 "
             "INNER JOIN ("
             "  SELECT server_name, MAX(timestamp) as max_ts "
             "  FROM server_performance GROUP BY server_name"
             ") p2 ON p1.server_name = p2.server_name AND p1.timestamp = "
             "p2.max_ts "
             "ORDER BY p1.score "
          << orderBy << " LIMIT " << pageSize << " OFFSET " << offset;
    if (mysql_query(conn_, query.str().c_str()) != 0)
    {
        std::cerr << "QueryManager: score rank query failed: "
                  << mysql_error(conn_) << std::endl;
        return records;
    }

    MYSQL_RES *res = mysql_store_result(conn_);
    if (!res) return records;
    auto now = std::chrono::system_clock::now();
    MYSQL_ROW row;
    while ((row = mysql_fetch_row(res)))
    {
        ServerScoreSummary rec;
        rec.server_name = row[0] ? row[0] : "";
        rec.score = row[1] ? std::atof(row[1]) : 0;
        rec.last_updated = row[2] ? parseTimeString(row[2]) : now;
        rec.cpu_percent = row[3] ? std::atof(row[3]) : 0;
        rec.mem_used_percent = row[4] ? std::atof(row[4]) : 0;
        rec.disk_util_percent = row[5] ? std::atof(row[5]) : 0;
        rec.load_avg_1 = row[6] ? std::atof(row[6]) : 0;

        // Determine server status based on last update time (e.g., if no update
        // for more than 60 seconds, consider it offline)
        auto age = std::chrono::duration_cast<std::chrono::seconds>(
                       now - rec.last_updated)
                       .count();
        rec.status = (age > 60) ? ServerStatus::OFFLINE : ServerStatus::ONLINE;

        records.push_back(rec);
    }
    mysql_free_result(res);
#else
    (void)order;
    (void)page;
    (void)pageSize;
    (void)totalCount;
#endif
    return records;
}

std::vector<ServerScoreSummary> QueryManager::queryLatestServerScores(
    ClusterStats *clusterStats)
{
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
    if (mysql_query(conn_, query.c_str()) != 0)
    {
        std::cerr << "QueryManager: latest score query failed: "
                  << mysql_error(conn_) << std::endl;
        return records;
    }

    MYSQL_RES *res = mysql_store_result(conn_);
    if (!res) return records;
    auto now = std::chrono::system_clock::now();
    float totalScore = 0;
    float maxScore = 0;
    float minScore = MAX_SCORE;
    std::string bestServer, worstServer;
    int onlineCount = 0, offlineCount = 0;

    MYSQL_ROW row;
    while ((row = mysql_fetch_row(res)))
    {
        ServerScoreSummary rec;
        rec.server_name = row[0] ? row[0] : "";
        rec.score = row[1] ? std::atof(row[1]) : 0;
        rec.last_updated = row[2] ? parseTimeString(row[2]) : now;
        rec.cpu_percent = row[3] ? std::atof(row[3]) : 0;
        rec.mem_used_percent = row[4] ? std::atof(row[4]) : 0;
        rec.disk_util_percent = row[5] ? std::atof(row[5]) : 0;
        rec.load_avg_1 = row[6] ? std::atof(row[6]) : 0;

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
        if (rec.score > maxScore)
        {
            maxScore = rec.score;
            bestServer = rec.server_name;
        }
        if (rec.score < minScore)
        {
            minScore = rec.score;
            worstServer = rec.server_name;
        }

        records.push_back(rec);
    }
    mysql_free_result(res);

    // Fill cluster stats if provided
    if (clusterStats)
    {
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
    int pageSize, int *totalCount)
{
    std::vector<NetDetailRecord> records;

#ifdef ENABLE_MYSQL
    std::lock_guard<std::mutex> lock(mutex_);
    if (!initialized_ || !conn_) return records;

    if (!validateTimeRange(range)) return records;
    if (page < 1) page = 1;
    if (pageSize < 1) pageSize = 100;

    std::string startTime = formatTimePoint(range.start_time);
    std::string endTime = formatTimePoint(range.end_time);

    // get total count of records
    std::ostringstream countQuery;
    countQuery << "SELECT COUNT(*) FROM server_net_detail WHERE server_name='"
               << serverName << "' AND timestamp BETWEEN '" << startTime
               << "' AND '" << endTime << "'";
    if (totalCount) *totalCount = getTotalCount(countQuery.str());

    // query net detail records with pagination
    int offset = (page - 1) * pageSize;
    std::ostringstream query;
    query << "SELECT server_name, net_name, timestamp, err_in, err_out, "
             "drop_in, drop_out, rcv_bytes_rate, snd_bytes_rate, "
             "rcv_packets_rate, snd_packets_rate "
             "FROM server_net_detail WHERE server_name='"
          << serverName << "' AND timestamp BETWEEN '" << endTime << "' AND '"
          << endTime << "' ORDER BY timestamp DESC LIMIT " << pageSize
          << " OFFSET " << offset;

    if (mysql_query(conn_, query.str().c_str()) != 0)
    {
        std::cerr << "QueryManager: net detail query failed: "
                  << mysql_error(conn_) << std::endl;
        return records;
    }

    MYSQL_RES *res = mysql_store_result(conn_);
    if (!res) return records;

    MYSQL_ROW row;
    while ((row = mysql_fetch_row(res)))
    {
        NetDetailRecord rec;
        int i = 0;
        rec.server_name = row[i++] ? row[i - 1] : "";
        rec.net_name = row[i++] ? row[i - 1] : "";
        rec.timestamp =
            row[i] ? parseTimeString(row[i]) : std::chrono::system_clock::now();
        i++;
        rec.err_in = row[i] ? std::stoull(row[i]) : 0;
        i++;
        rec.err_out = row[i] ? std::stoull(row[i]) : 0;
        i++;
        rec.drop_in = row[i] ? std::stoull(row[i]) : 0;
        i++;
        rec.drop_out = row[i] ? std::stoull(row[i]) : 0;
        i++;
        rec.recv_bytes_rate = row[i] ? std::atof(row[i]) : 0;
        i++;
        rec.sent_bytes_rate = row[i] ? std::atof(row[i]) : 0;
        i++;
        rec.recv_packets_rate = row[i] ? std::atof(row[i]) : 0;
        i++;
        rec.sent_packets_rate = row[i] ? std::atof(row[i]) : 0;
        records.push_back(rec);
    }
    mysql_free_result(res);
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
    int pageSize, int *totalCount)
{
    std::vector<DiskDetailRecord> records;

#ifdef ENABLE_MYSQL
    std::lock_guard<std::mutex> lock(mutex_);
    if (!initialized_ || !conn_) return records;

    if (!validateTimeRange(range)) return records;
    if (page < 1) page = 1;
    if (pageSize < 1) pageSize = 100;

    std::string startTime = formatTimePoint(range.start_time);
    std::string endTime = formatTimePoint(range.end_time);

    std::ostringstream count_sql;
    count_sql << "SELECT COUNT(*) FROM server_disk_detail WHERE server_name='"
              << serverName << "' AND timestamp BETWEEN '" << startTime
              << "' AND '" << endTime << "'";
    if (totalCount) *totalCount = getTotalCount(count_sql.str());

    int offset = (page - 1) * pageSize;
    std::ostringstream query;
    query << "SELECT server_name, disk_name, timestamp, read_bytes_per_sec, "
             "write_bytes_per_sec, read_iops, write_iops, avg_read_latency_ms, "
             "avg_write_latency_ms, util_percent "
             "FROM server_disk_detail WHERE server_name='"
          << serverName << "' AND timestamp BETWEEN '" << startTime << "' AND '"
          << endTime << "' ORDER BY timestamp DESC LIMIT " << pageSize
          << " OFFSET " << offset;

    if (mysql_query(conn_, query.str().c_str()) != 0)
    {
        std::cerr << "QueryManager: disk detail query failed: "
                  << mysql_error(conn_) << std::endl;
        return records;
    }

    MYSQL_RES *res = mysql_store_result(conn_);
    if (!res) return records;

    MYSQL_ROW row;
    while ((row = mysql_fetch_row(res)))
    {
        DiskDetailRecord rec;
        int i = 0;
        rec.server_name = row[i++] ? row[i - 1] : "";
        rec.disk_name = row[i++] ? row[i - 1] : "";
        rec.timestamp =
            row[i] ? parseTimeString(row[i]) : std::chrono::system_clock::now();
        i++;
        rec.read_bytes_per_sec = row[i] ? std::atof(row[i]) : 0;
        i++;
        rec.write_bytes_per_sec = row[i] ? std::atof(row[i]) : 0;
        i++;
        rec.read_iops = row[i] ? std::atof(row[i]) : 0;
        i++;
        rec.write_iops = row[i] ? std::atof(row[i]) : 0;
        i++;
        rec.avg_read_latency_ms = row[i] ? std::atof(row[i]) : 0;
        i++;
        rec.avg_write_latency_ms = row[i] ? std::atof(row[i]) : 0;
        i++;
        rec.util_percent = row[i] ? std::atof(row[i]) : 0;
        records.push_back(rec);
    }
    mysql_free_result(res);
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
    int pageSize, int *totalCount)
{
    std::vector<MemDetailRecord> records;

#ifdef ENABLE_MYSQL
    std::lock_guard<std::mutex> lock(mutex_);
    if (!initialized_ || !conn_) return records;

    if (!validateTimeRange(range)) return records;
    if (page < 1) page = 1;
    if (pageSize < 1) pageSize = 100;

    std::string startTime = formatTimePoint(range.start_time);
    std::string endTime = formatTimePoint(range.end_time);

    std::ostringstream count_sql;
    count_sql << "SELECT COUNT(*) FROM server_mem_detail WHERE server_name='"
              << serverName << "' AND timestamp BETWEEN '" << startTime
              << "' AND '" << endTime << "'";
    if (totalCount) *totalCount = getTotalCount(count_sql.str());

    int offset = (page - 1) * pageSize;
    std::ostringstream query;
    query << "SELECT server_name, timestamp, total, free, avail, buffers, "
             "cached, active, inactive, dirty "
             "FROM server_mem_detail WHERE server_name='"
          << serverName << "' AND timestamp BETWEEN '" << startTime << "' AND '"
          << serverName << "' ORDER BY timestamp DESC LIMIT " << pageSize
          << " OFFSET " << offset;

    if (mysql_query(conn_, query.str().c_str()) != 0)
    {
        std::cerr << "QueryManager: mem detail query failed: "
                  << mysql_error(conn_) << std::endl;
        return records;
    }

    MYSQL_RES *res = mysql_store_result(conn_);
    if (!res) return records;

    MYSQL_ROW row;
    while ((row = mysql_fetch_row(res)))
    {
        MemDetailRecord rec;
        int i = 0;
        rec.server_name = row[i++] ? row[i - 1] : "";
        rec.timestamp =
            row[i] ? parseTimeString(row[i]) : std::chrono::system_clock::now();
        i++;
        rec.mem_total = row[i] ? std::atof(row[i]) : 0;
        i++;
        rec.mem_free = row[i] ? std::atof(row[i]) : 0;
        i++;
        rec.mem_avail = row[i] ? std::atof(row[i]) : 0;
        i++;
        rec.buffers = row[i] ? std::atof(row[i]) : 0;
        i++;
        rec.cached = row[i] ? std::atof(row[i]) : 0;
        i++;
        rec.active = row[i] ? std::atof(row[i]) : 0;
        i++;
        rec.inactive = row[i] ? std::atof(row[i]) : 0;
        i++;
        rec.dirty = row[i] ? std::atof(row[i]) : 0;
        records.push_back(rec);
    }
    mysql_free_result(res);
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
    int pageSize, int *totalCount)
{
    std::vector<SoftIrqDetailRecord> records;

#ifdef ENABLE_MYSQL
    std::lock_guard<std::mutex> lock(mutex_);
    if (!initialized_ || !conn_) return records;

    if (!validateTimeRange(range)) return records;
    if (page < 1) page = 1;
    if (pageSize < 1) pageSize = 100;

    std::string startTime = formatTimePoint(range.start_time);
    std::string endTime = formatTimePoint(range.end_time);

    std::ostringstream count_sql;
    count_sql
        << "SELECT COUNT(*) FROM server_softirq_detail WHERE server_name='"
        << serverName << "' AND timestamp BETWEEN '" << startTime << "' AND '"
        << endTime << "'";
    if (totalCount) *totalCount = getTotalCount(count_sql.str());

    int offset = (page - 1) * pageSize;
    std::ostringstream query;
    query << "SELECT server_name, cpu_name, timestamp, hi, timer, net_tx, "
             "net_rx, block, sched "
             "FROM server_softirq_detail WHERE server_name='"
          << serverName << "' AND timestamp BETWEEN '" << startTime << "' AND '"
          << endTime << "' ORDER BY timestamp DESC LIMIT " << pageSize
          << " OFFSET " << offset;

    if (mysql_query(conn_, query.str().c_str()) != 0)
    {
        std::cerr << "QueryManager: softirq detail query failed: "
                  << mysql_error(conn_) << std::endl;
        return records;
    }

    MYSQL_RES *res = mysql_store_result(conn_);
    if (!res) return records;

    MYSQL_ROW row;
    while ((row = mysql_fetch_row(res)))
    {
        SoftIrqDetailRecord rec;
        int i = 0;
        rec.server_name = row[i++] ? row[i - 1] : "";
        rec.cpu_name = row[i++] ? row[i - 1] : "";
        rec.timestamp =
            row[i] ? parseTimeString(row[i]) : std::chrono::system_clock::now();
        i++;
        rec.hi = row[i] ? std::stoll(row[i]) : 0;
        i++;
        rec.timer = row[i] ? std::stoll(row[i]) : 0;
        i++;
        rec.net_tx = row[i] ? std::stoll(row[i]) : 0;
        i++;
        rec.net_rx = row[i] ? std::stoll(row[i]) : 0;
        i++;
        rec.block = row[i] ? std::stoll(row[i]) : 0;
        i++;
        rec.sched = row[i] ? std::stoll(row[i]) : 0;
        records.push_back(rec);
    }
    mysql_free_result(res);
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