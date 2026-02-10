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

namespace monitor
{
MonitorPusher::MonitorPusher(const std::string &managerAddress,
                             int intervalSeconds)
    : managerAddress_(managerAddress),
      intervalSeconds_(intervalSeconds),
      running_(false)
{
    auto channel = grpc::CreateChannel(managerAddress_,
                                       grpc::InsecureChannelCredentials());
    stub_ = monitor::proto::GrpcManager::NewStub(channel);
    collector_ = std::make_unique<MetricCollector>();
}

MonitorPusher::~MonitorPusher() { stop(); }

void MonitorPusher::start()
{
    if (running_) return;

    running_ = true;
    thread_ = std::make_unique<std::thread>(&MonitorPusher::pushLoop, this);
    std::cout << "MonitorPusher started, pushing to " << managerAddress_
              << " every " << intervalSeconds_ << " seconds." << std::endl;
}

void MonitorPusher::stop()
{
    running_ = false;
    if (thread_ && thread_->joinable()) thread_->join();
}

void MonitorPusher::pushLoop()
{
    while (running_)
    {
        // Collect metrics data
        if (!pushOnce())
        {
            std::cerr << "Failed to push metrics data to " << managerAddress_
                      << std::endl;
        }

        // Sleep for the specified interval, but check running_ every second
        for (int i = 0; i < intervalSeconds_ && running_; ++i)
            std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}

bool MonitorPusher::pushOnce()
{
    monitor::proto::MonitorInfo info;
    // print collected metrics
    std::cout << "\n============== Collected Metrics ============="
              << std::endl;

    // host name
    if (info.has_host_info())
    {
        std::cout << "[Host] Hostname: " << info.host_info().hostname()
                  << std::endl;
    }

    // cpu statistics
    std::cout << "\n--- CPU Statistics ---" << std::endl;
    for (int i = 0; i < info.cpu_stat_size(); ++i)
    {
        const auto &cpuStat = info.cpu_stat(i);
        std::cout << "[" << cpuStat.cpu_name() << "] "
                  << "Total: " << cpuStat.cpu_percent() << "%, "
                  << "User: " << cpuStat.usr_percent() << "%, "
                  << "System: " << cpuStat.system_percent() << "%, "
                  << "Nice: " << cpuStat.nice_percent() << "%, "
                  << "Idle: " << cpuStat.idle_percent() << "%, "
                  << "IOWait: " << cpuStat.io_wait_percent() << "%, "
                  << "IRQ: " << cpuStat.irq_percent() << "%, "
                  << "SoftIRQ: " << cpuStat.soft_irq_percent() << "%"
                  << std::endl;
    }

    // CPU load
    if (info.has_cpu_load())
    {
        std::cout << "\n--- CPU Load ---" << std::endl;
        std::cout << "[Load] 1min: " << info.cpu_load().load_avg_1()
                  << ", 5min: " << info.cpu_load().load_avg_3()
                  << ", 15min: " << info.cpu_load().load_avg_15() << std::endl;
    }

    // memory info
    if (info.has_mem_info())
    {
        const auto &mem = info.mem_info();
        std::cout << "\n--- Memory Info ---" << std::endl;
        std::cout << "[Memory] Used: " << mem.used_percent() << "%"
                  << std::endl;
        std::cout << "  Total: " << mem.total() << " MB, "
                  << "Free: " << mem.free() << " MB, "
                  << "Avail: " << mem.avail() << " MB" << std::endl;
        std::cout << "  Buffers: " << mem.buffers() << " MB, "
                  << "Cached: " << mem.cached() << " MB, "
                  << "SwapCached: " << mem.swap_cached() << " MB" << std::endl;
        std::cout << "  Active: " << mem.active() << " MB, "
                  << "Inactive: " << mem.inactive() << " MB" << std::endl;
        std::cout << "  ActiveAnon: " << mem.active_anon() << " MB, "
                  << "InactiveAnon: " << mem.inactive_anon() << " MB"
                  << std::endl;
        std::cout << "  ActiveFile: " << mem.active_file() << " MB, "
                  << "InactiveFile: " << mem.inactive_file() << " MB"
                  << std::endl;
        std::cout << "  Dirty: " << mem.dirty() << " MB, "
                  << "Writeback: " << mem.writeback() << " MB" << std::endl;
        std::cout << "  AnonPages: " << mem.anon_pages() << " MB, "
                  << "Mapped: " << mem.mapped() << " MB" << std::endl;
        std::cout << "  KReclaimable: " << mem.kreclaimable() << " MB, "
                  << "SReclaimable: " << mem.sreclaimable() << " MB, "
                  << "SUnreclaim: " << mem.sunreclaim() << " MB" << std::endl;
    }

    // net info
    if (info.net_info_size() > 0)
    {
        std::cout << "\n--- Network Info ---" << std::endl;
        for (int i = 0; i < info.net_info_size(); ++i)
        {
            const auto &net = info.net_info(i);
            std::cout << "[" << net.name() << "]" << std::endl;
            std::cout << "  Recv: " << net.rcv_rate() << " B/s ("
                      << net.rcv_packets_rate() << " pkt/s)" << std::endl;
            std::cout << "  Send: " << net.send_rate() << " B/s ("
                      << net.send_packets_rate() << " pkt/s)" << std::endl;
            std::cout << "  Errors(in/out): " << net.err_in() << "/"
                      << net.err_out() << ", Drops(in/out): " << net.drop_in()
                      << "/" << net.drop_out() << std::endl;
        }
    }

    // disk info
    if (info.disk_info_size() > 0)
    {
        std::cout << "\n--- Disk Info ---" << std::endl;
        for (int i = 0; i < info.disk_info_size(); ++i)
        {
            const auto &disk = info.disk_info(i);
            std::cout << "[" << disk.name() << "]" << std::endl;
            std::cout << "  Read: " << disk.read_bytes_per_sec() / 1024.0
                      << " KB/s, "
                      << "IOPS: " << disk.read_iops() << ", "
                      << "Latency: " << disk.avg_read_latency_ms() << " ms"
                      << std::endl;
            std::cout << "  Write: " << disk.write_bytes_per_sec() / 1024.0
                      << " KB/s, "
                      << "IOPS: " << disk.write_iops() << ", "
                      << "Latency: " << disk.avg_write_latency_ms() << " ms"
                      << std::endl;
            std::cout << "  Util: " << disk.util_percent() << "%, "
                      << "IO_InProgress: " << disk.io_in_progress()
                      << std::endl;
            std::cout << "  Reads: " << disk.reads() << ", "
                      << "Writes: " << disk.writes() << ", "
                      << "SectorsRead: " << disk.sectors_read() << ", "
                      << "SectorsWritten: " << disk.sectors_written()
                      << std::endl;
        }
    }

    // softirq info
    if (info.soft_irq_size() > 0)
    {
        std::cout << "\n--- SoftIRQ Info ---" << std::endl;
        for (int i = 0; i < info.soft_irq_size(); ++i)
        {
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

    std::cout << "==============================================" << std::endl;

    // push to manager
    grpc::ClientContext context;
    google::protobuf::Empty response;
    grpc::Status status = stub_->SetMonitorInfo(&context, info, &response);

    if (status.ok())
    {
        std::cout << ">>> Pushed monitor data to " << managerAddress_
                  << " successfully <<<" << std::endl;
        return true;
    }
    else
    {
        std::cerr << ">>> Push failed: " << status.error_message() << " <<<"
                  << std::endl;
        return false;
    }
}

} // namespace monitor