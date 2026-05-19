#include "MonitorPusher.h"

#include <google/protobuf/empty.pb.h>
#include <grpcpp/grpcpp.h>
#include <iostream>
#include <chrono>
#include <memory>
#include <thread>
#include "MetricCollector.h"
#include "monitor_info.grpc.pb.h"
#include "monitor_info.pb.h"

namespace monitor {
/**
 * @brief         构造监控推送器，初始化 gRPC stub 和指标采集器
 *
 * @param         managerAddress manager 的 gRPC 地址
 * @param         intervalSeconds 推送间隔，单位秒
 */
MonitorPusher::MonitorPusher(const std::string &managerAddress, int intervalSeconds)
    : managerAddress_(managerAddress), intervalSeconds_(intervalSeconds), running_(false) {
    auto channel = grpc::CreateChannel(managerAddress_, grpc::InsecureChannelCredentials());
    stub_ = monitor::proto::GrpcManager::NewStub(channel);
    collector_ = std::make_unique<MetricCollector>();
}

/**
 * @brief         析构监控推送器并停止后台推送线程
 *
 */
MonitorPusher::~MonitorPusher() { stop(); }

/**
 * @brief         启动后台推送线程
 *
 */
void MonitorPusher::start() {
    if (running_) return;

    running_ = true;
    thread_ = std::make_unique<std::thread>(&MonitorPusher::pushLoop, this);
    std::cout << "MonitorPusher started, pushing to " << managerAddress_ << " every " << intervalSeconds_ << " seconds."
              << std::endl;
}

/**
 * @brief         停止后台推送线程
 *
 */
void MonitorPusher::stop() {
    running_ = false;
    if (thread_ && thread_->joinable()) thread_->join();
}

/**
 * @brief         按固定间隔循环采集并推送监控数据
 *
 */
void MonitorPusher::pushLoop() {
    while (running_) {
        // 采集并推送指标数据
        if (!pushOnce()) {
            std::cerr << "Failed to push metrics data to " << managerAddress_ << std::endl;
        }

        // 按指定间隔休眠，但每秒检查一次 running_ 状态
        for (int i = 0; i < intervalSeconds_ && running_; ++i) std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}

/**
 * @brief         执行一次指标采集、日志打印和 gRPC 推送
 *
 * @return        推送成功返回 true，否则返回 false
 */
bool MonitorPusher::pushOnce() {
    monitor::proto::MonitorInfo info;
    collector_->collectAll(&info);
    // 打印采集到的指标
    std::cout << "\n============== Collected Metrics =============" << std::endl;

    // 主机名
    if (info.has_host_info()) {
        std::cout << "[Host] Hostname: " << info.host_info().hostname() << std::endl;
    }

    // CPU 统计
    std::cout << "\n--- CPU Statistics ---" << std::endl;
    for (int i = 0; i < info.cpu_stat_size(); ++i) {
        const auto &cpuStat = info.cpu_stat(i);
        std::cout << "[" << cpuStat.cpu_name() << "] "
                  << "Total: " << cpuStat.cpu_percent() << "%, "
                  << "User: " << cpuStat.usr_percent() << "%, "
                  << "System: " << cpuStat.system_percent() << "%, "
                  << "Nice: " << cpuStat.nice_percent() << "%, "
                  << "Idle: " << cpuStat.idle_percent() << "%, "
                  << "IOWait: " << cpuStat.io_wait_percent() << "%, "
                  << "IRQ: " << cpuStat.irq_percent() << "%, "
                  << "SoftIRQ: " << cpuStat.soft_irq_percent() << "%" << std::endl;
    }

    // CPU 负载
    if (info.has_cpu_load()) {
        std::cout << "\n--- CPU Load ---" << std::endl;
        std::cout << "[Load] 1min: " << info.cpu_load().load_avg_1() << ", 5min: " << info.cpu_load().load_avg_3()
                  << ", 15min: " << info.cpu_load().load_avg_15() << std::endl;
    }

    // 内存信息
    if (info.has_mem_info()) {
        const auto &mem = info.mem_info();
        std::cout << "\n--- Memory Info ---" << std::endl;
        std::cout << "[Memory] Used: " << mem.used_percent() << "%" << std::endl;
        std::cout << "  Total: " << mem.total() << " MB, "
                  << "Free: " << mem.free() << " MB, "
                  << "Avail: " << mem.avail() << " MB" << std::endl;
        std::cout << "  Buffers: " << mem.buffers() << " MB, "
                  << "Cached: " << mem.cached() << " MB, "
                  << "SwapCached: " << mem.swap_cached() << " MB" << std::endl;
        std::cout << "  Active: " << mem.active() << " MB, "
                  << "Inactive: " << mem.inactive() << " MB" << std::endl;
        std::cout << "  ActiveAnon: " << mem.active_anon() << " MB, "
                  << "InactiveAnon: " << mem.inactive_anon() << " MB" << std::endl;
        std::cout << "  ActiveFile: " << mem.active_file() << " MB, "
                  << "InactiveFile: " << mem.inactive_file() << " MB" << std::endl;
        std::cout << "  Dirty: " << mem.dirty() << " MB, "
                  << "Writeback: " << mem.writeback() << " MB" << std::endl;
        std::cout << "  AnonPages: " << mem.anon_pages() << " MB, "
                  << "Mapped: " << mem.mapped() << " MB" << std::endl;
        std::cout << "  KReclaimable: " << mem.kreclaimable() << " MB, "
                  << "SReclaimable: " << mem.sreclaimable() << " MB, "
                  << "SUnreclaim: " << mem.sunreclaim() << " MB" << std::endl;
    }

    // 网络信息
    if (info.net_info_size() > 0) {
        std::cout << "\n--- Network Info ---" << std::endl;
        for (int i = 0; i < info.net_info_size(); ++i) {
            const auto &net = info.net_info(i);
            std::cout << "[" << net.name() << "]" << std::endl;
            std::cout << "  Recv: " << net.rcv_rate() << " B/s (" << net.rcv_packets_rate() << " pkt/s)" << std::endl;
            std::cout << "  Send: " << net.send_rate() << " B/s (" << net.send_packets_rate() << " pkt/s)" << std::endl;
            std::cout << "  Errors(in/out): " << net.err_in() << "/" << net.err_out()
                      << ", Drops(in/out): " << net.drop_in() << "/" << net.drop_out() << std::endl;
        }
    }

    // 磁盘信息
    if (info.disk_info_size() > 0) {
        std::cout << "\n--- Disk Info ---" << std::endl;
        for (int i = 0; i < info.disk_info_size(); ++i) {
            const auto &disk = info.disk_info(i);
            std::cout << "[" << disk.name() << "]" << std::endl;
            std::cout << "  Read: " << disk.read_bytes_per_sec() / 1024.0 << " KB/s, "
                      << "IOPS: " << disk.read_iops() << ", "
                      << "Latency: " << disk.avg_read_latency_ms() << " ms" << std::endl;
            std::cout << "  Write: " << disk.write_bytes_per_sec() / 1024.0 << " KB/s, "
                      << "IOPS: " << disk.write_iops() << ", "
                      << "Latency: " << disk.avg_write_latency_ms() << " ms" << std::endl;
            std::cout << "  Util: " << disk.util_percent() << "%, "
                      << "IO_InProgress: " << disk.io_in_progress() << std::endl;
            std::cout << "  Reads: " << disk.reads() << ", "
                      << "Writes: " << disk.writes() << ", "
                      << "SectorsRead: " << disk.sectors_read() << ", "
                      << "SectorsWritten: " << disk.sectors_written() << std::endl;
        }
    }

    // 软中断信息
    if (info.soft_irq_size() > 0) {
        std::cout << "\n--- SoftIRQ Info ---" << std::endl;
        for (int i = 0; i < info.soft_irq_size(); ++i) {
            const auto &sirq = info.soft_irq(i);
            std::cout << "[" << sirq.cpu() << "] "
                      << "HI: " << sirq.hi() << ", "
                      << "TIMER: " << sirq.timer() << ", "
                      << "NET_TX: " << sirq.net_tx() << ", "
                      << "NET_RX: " << sirq.net_rx() << ", "
                      << "BLOCK: " << sirq.block() << ", "
                      << "IRQ_POLL: " << sirq.irq_poll() << ", "
                      << "TASKLET: " << sirq.tasklet() << ", "
                      << "SCHED: " << sirq.sched() << ", "
                      << "HRTIMER: " << sirq.hrtimer() << ", "
                      << "RCU: " << sirq.rcu() << std::endl;
        }
    }

    // MySQL 信息
    if (info.mysql_info_size() > 0) {
        std::cout << "\n--- MySQL Info ---" << std::endl;
        for (int i = 0; i < info.mysql_info_size(); ++i) {
            const auto &mysql = info.mysql_info(i);
            std::cout << "[" << mysql.instance() << "] "
                      << "Up: " << (mysql.up() ? "yes" : "no") << ", "
                      << "Role: " << mysql.role() << ", "
                      << "Version: " << mysql.version() << std::endl;
            std::cout << "  Connections: " << mysql.threads_connected() << "/" << mysql.max_connections()
                      << ", Running: " << mysql.threads_running() << ", Questions: " << mysql.questions()
                      << ", Slow: " << mysql.slow_queries() << std::endl;
            std::cout << "  InnoDB hit: " << mysql.innodb_buffer_pool_hit_percent()
                      << "%, Row lock waits: " << mysql.innodb_row_lock_waits() << std::endl;
            if (mysql.replication_configured()) {
                std::cout << "  Replication: " << (mysql.replication_running() ? "running" : "stopped")
                          << ", Lag: " << mysql.replication_lag_seconds() << "s" << std::endl;
            }
        }
    }

    std::cout << "==============================================" << std::endl;

    // 推送到 manager
    grpc::ClientContext context;
    google::protobuf::Empty response;
    grpc::Status status = stub_->SetMonitorInfo(&context, info, &response);

    if (status.ok()) {
        std::cout << ">>> Pushed monitor data to " << managerAddress_ << " successfully <<<" << std::endl;
        return true;
    } else {
        std::cerr << ">>> Push failed: " << status.error_message() << " <<<" << std::endl;
        return false;
    }
}

} // namespace monitor
