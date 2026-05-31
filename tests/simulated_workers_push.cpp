#include <google/protobuf/empty.pb.h>
#include <grpcpp/grpcpp.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <memory>
#include <random>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include "monitor_info.grpc.pb.h"
#include "monitor_info.pb.h"

namespace {

struct WorkerConfig {
    int id = 1;
    std::string manager_address;
    int interval_seconds = 2;
    int rounds = 5;
};

std::string workerName(int id) {
    std::ostringstream os;
    os << "worker-" << std::setw(2) << std::setfill('0') << id;
    return os.str();
}

std::string workerIp(int id) { return "10.10.0." + std::to_string(100 + id); }

// 产生一个在 [min, max) 范围内的随机浮点数
float nextFloat(std::mt19937 &rng, float min, float max) {
    std::uniform_real_distribution<float> dist(min, max);
    return dist(rng);
}

void fillCpuStat(monitor::proto::MonitorInfo *info, std::mt19937 &rng, int worker_id, int round) {
    auto add_cpu = [&](const std::string &name, float base) {
        auto *cpu = info->add_cpu_stat();
        // CPU 使用率在 1% 到 98% 之间，受 worker_id 和 round
        // 的影响，并有一定随机波动
        const float cpu_percent = std::min(98.0f, std::max(1.0f, base + nextFloat(rng, -5.0f, 5.0f)));
        const float usr = cpu_percent * nextFloat(rng, 0.45f, 0.60f);
        const float system = cpu_percent * nextFloat(rng, 0.20f, 0.35f);
        const float nice = cpu_percent * nextFloat(rng, 0.00f, 0.03f);
        const float irq = cpu_percent * nextFloat(rng, 0.00f, 0.02f);
        const float soft_irq = cpu_percent * nextFloat(rng, 0.01f, 0.04f);
        const float io_wait = std::max(0.0f, cpu_percent - usr - system - nice - irq - soft_irq);

        cpu->set_cpu_name(name);
        cpu->set_cpu_percent(cpu_percent);
        cpu->set_usr_percent(usr);
        cpu->set_system_percent(system);
        cpu->set_nice_percent(nice);
        cpu->set_idle_percent(100.0f - cpu_percent);
        cpu->set_io_wait_percent(io_wait);
        cpu->set_irq_percent(irq);
        cpu->set_soft_irq_percent(soft_irq);
    };

    // 根据 worker_id 和 round 生成一个 base 值，使得不同 worker 和不同轮次的
    // CPU 使用率有一定规律但又不完全相同
    const float base = 18.0f + worker_id * 8.0f + (round % 4) * 3.0f;
    add_cpu("cpu", base);
    // 假设每个 worker 有 4 个 CPU 核心，核心的使用率在 base 的基础上有一定差异
    for (int core = 0; core < 4; ++core) add_cpu("cpu" + std::to_string(core), base + core * 1.5f);
}

void fillCpuLoad(monitor::proto::MonitorInfo *info, std::mt19937 &rng, int worker_id, int round) {
    auto *load = info->mutable_cpu_load();
    const float base = 0.4f + worker_id * 0.25f + (round % 3) * 0.15f;
    load->set_load_avg_1(base + nextFloat(rng, 0.0f, 0.25f));
    load->set_load_avg_3(base + nextFloat(rng, 0.0f, 0.20f));
    load->set_load_avg_15(base + nextFloat(rng, 0.0f, 0.15f));
}

void fillMemory(monitor::proto::MonitorInfo *info, std::mt19937 &rng, int worker_id, int round) {
    auto *mem = info->mutable_mem_info();
    // 总内存固定为 32GB，使用率根据 worker_id 和 round 生成一个在 25% 到 92%
    // 之间的值，并有一定随机波动
    const float total = 32768.0f;
    const float used_percent = std::min(92.0f, 25.0f + worker_id * 8.0f + round * 1.2f + nextFloat(rng, -2.0f, 2.0f));
    const float used = total * used_percent / 100.0f;
    const float free = total - used;
    const float cached = total * nextFloat(rng, 0.12f, 0.20f);
    const float buffers = total * nextFloat(rng, 0.01f, 0.03f);

    mem->set_total(total);
    mem->set_free(free);
    mem->set_avail(std::min(total, free + cached + buffers));
    mem->set_buffers(buffers);
    mem->set_cached(cached);
    mem->set_swap_cached(nextFloat(rng, 0.0f, 128.0f));
    mem->set_active(used * 0.55f);
    mem->set_inactive(used * 0.20f);
    mem->set_active_anon(used * 0.35f);
    mem->set_inactive_anon(used * 0.10f);
    mem->set_active_file(used * 0.20f);
    mem->set_inactive_file(used * 0.10f);
    mem->set_dirty(nextFloat(rng, 20.0f, 200.0f));
    mem->set_writeback(nextFloat(rng, 0.0f, 20.0f));
    mem->set_anon_pages(used * 0.32f);
    mem->set_mapped(used * 0.08f);
    mem->set_kreclaimable(total * 0.03f);
    mem->set_sreclaimable(total * 0.02f);
    mem->set_sunreclaim(total * 0.01f);
    mem->set_used_percent(used_percent);
}

void fillNet(monitor::proto::MonitorInfo *info, std::mt19937 &rng, int worker_id, int round) {
    auto *net = info->add_net_info();
    // 假设每个 worker 有一个名为 eth0 的网卡，发送速率和接收速率根据 worker_id
    // 和 round 生成一个在 600Mbps 到 1200Mbps 之间的值，并有一定随机波动
    net->set_name("eth0");
    net->set_send_rate(600.0f + worker_id * 95.0f + round * 20.0f + nextFloat(rng, 0.0f, 80.0f));
    net->set_rcv_rate(900.0f + worker_id * 130.0f + round * 35.0f + nextFloat(rng, 0.0f, 120.0f));
    net->set_send_packets_rate(500.0f + worker_id * 20.0f + nextFloat(rng, 0.0f, 50.0f));
    net->set_rcv_packets_rate(700.0f + worker_id * 30.0f + nextFloat(rng, 0.0f, 70.0f));
    // 每隔几轮模拟一次错误和丢包，worker_id 5 的发送偶尔出错，worker_id 4
    // 的接收偶尔丢包
    net->set_err_in(worker_id == 5 && round % 4 == 0 ? 1 : 0);
    net->set_err_out(0);
    net->set_drop_in(worker_id == 4 && round % 5 == 0 ? 2 : 0);
    net->set_drop_out(0);
}

void fillDisk(monitor::proto::MonitorInfo *info, std::mt19937 &rng, int worker_id, int round) {
    auto *disk = info->add_disk_info();
    disk->set_name("sda");
    disk->set_reads(100000 + worker_id * 1000 + round * 80);
    disk->set_writes(80000 + worker_id * 900 + round * 70);
    disk->set_sectors_read(2000000 + worker_id * 10000 + round * 4096);
    disk->set_sectors_written(1500000 + worker_id * 9000 + round * 3072);
    disk->set_read_time_ms(20000 + worker_id * 500 + round * 30);
    disk->set_write_time_ms(24000 + worker_id * 450 + round * 25);
    disk->set_io_in_progress(static_cast<uint64_t>(round % 3));
    disk->set_io_time_ms(30000 + worker_id * 300 + round * 20);
    disk->set_weighted_io_time_ms(45000 + worker_id * 500 + round * 40);
    disk->set_read_bytes_per_sec(2.0 * 1024 * 1024 + worker_id * 128 * 1024 + nextFloat(rng, 0.0f, 256.0f * 1024));
    disk->set_write_bytes_per_sec(1.5 * 1024 * 1024 + worker_id * 96 * 1024 + nextFloat(rng, 0.0f, 200.0f * 1024));
    disk->set_read_iops(100.0 + worker_id * 8.0 + nextFloat(rng, 0.0f, 20.0f));
    disk->set_write_iops(80.0 + worker_id * 7.0 + nextFloat(rng, 0.0f, 15.0f));
    disk->set_avg_read_latency_ms(1.0 + worker_id * 0.2 + nextFloat(rng, 0.0f, 0.8f));
    disk->set_avg_write_latency_ms(1.4 + worker_id * 0.25 + nextFloat(rng, 0.0f, 1.0f));
    disk->set_util_percent(std::min(95.0, 10.0 + worker_id * 7.0 + round * 1.5 + nextFloat(rng, 0.0f, 6.0f)));
}

void fillSoftIrq(monitor::proto::MonitorInfo *info, std::mt19937 &rng) {
    for (int core = 0; core < 4; ++core) {
        auto *soft_irq = info->add_soft_irq();
        soft_irq->set_cpu("cpu" + std::to_string(core));
        soft_irq->set_hi(nextFloat(rng, 0.0f, 1.0f));
        soft_irq->set_timer(nextFloat(rng, 1.0f, 4.0f));
        soft_irq->set_net_tx(nextFloat(rng, 0.5f, 3.0f));
        soft_irq->set_net_rx(nextFloat(rng, 0.5f, 4.0f));
        soft_irq->set_block(nextFloat(rng, 0.0f, 1.5f));
        soft_irq->set_irq_poll(nextFloat(rng, 0.0f, 0.8f));
        soft_irq->set_tasklet(nextFloat(rng, 0.0f, 1.0f));
        soft_irq->set_sched(nextFloat(rng, 0.5f, 2.5f));
        soft_irq->set_hrtimer(nextFloat(rng, 0.0f, 1.0f));
        soft_irq->set_rcu(nextFloat(rng, 0.5f, 2.0f));
    }
}

void fillMysql(monitor::proto::MonitorInfo *info, std::mt19937 &rng, int worker_id, int round) {
    auto *mysql = info->add_mysql_info();
    const std::string host = workerIp(worker_id);
    const uint64_t counter_base = 1000000ULL + static_cast<uint64_t>(worker_id) * 50000ULL;
    const uint64_t round_delta = static_cast<uint64_t>(round) * 1200ULL;
    const uint64_t questions = counter_base + round_delta;
    const uint64_t com_select = counter_base / 2ULL + round_delta * 65ULL / 100ULL;
    const uint64_t com_insert = counter_base / 8ULL + round_delta * 12ULL / 100ULL;
    const uint64_t com_update = counter_base / 10ULL + round_delta * 10ULL / 100ULL;
    const uint64_t com_delete = counter_base / 20ULL + round_delta * 3ULL / 100ULL;
    const uint64_t com_commit = counter_base / 6ULL + round_delta * 16ULL / 100ULL;
    const uint64_t com_rollback = counter_base / 200ULL + round_delta / 100ULL;
    const uint64_t buffer_reads =
        600ULL + static_cast<uint64_t>(worker_id) * 20ULL + static_cast<uint64_t>(round) * 2ULL;
    const uint64_t buffer_requests = buffer_reads * 450ULL;

    mysql->set_instance(host + ":3306");
    mysql->set_host(host);
    mysql->set_port(3306);
    mysql->set_up(true);
    mysql->set_version("8.0.36");
    mysql->set_role(worker_id % 5 == 0 ? "replica" : "primary");
    mysql->set_max_connections(2000);
    mysql->set_threads_connected(80 + worker_id * 7 + round % 20);
    mysql->set_threads_running(4 + worker_id % 8 + round % 5);
    mysql->set_aborted_connects(static_cast<uint64_t>(worker_id / 3 + round / 20));
    mysql->set_questions(questions);
    mysql->set_com_select(com_select);
    mysql->set_com_insert(com_insert);
    mysql->set_com_update(com_update);
    mysql->set_com_delete(com_delete);
    mysql->set_com_commit(com_commit);
    mysql->set_com_rollback(com_rollback);
    mysql->set_slow_queries(20 + static_cast<uint64_t>(worker_id) * 2ULL + static_cast<uint64_t>(round / 4));
    mysql->set_innodb_buffer_pool_read_requests(buffer_requests);
    mysql->set_innodb_buffer_pool_reads(buffer_reads);
    mysql->set_innodb_buffer_pool_hit_percent(99.0 + nextFloat(rng, 0.0f, 0.7f));
    mysql->set_innodb_row_lock_waits(40 + static_cast<uint64_t>(worker_id) * 3ULL +
                                     static_cast<uint64_t>(round / 3));
    mysql->set_innodb_row_lock_time_avg_ms(1.0 + worker_id * 0.15 + nextFloat(rng, 0.0f, 0.8f));
    mysql->set_replication_configured(worker_id % 5 == 0);
    mysql->set_replication_running(worker_id % 5 == 0);
    mysql->set_replication_lag_seconds(worker_id % 5 == 0 ? nextFloat(rng, 0.0f, 2.5f) : 0.0);
}

void fillRedis(monitor::proto::MonitorInfo *info, std::mt19937 &rng, int worker_id, int round) {
    auto *redis = info->add_redis_info();
    const std::string host = workerIp(worker_id);
    const uint64_t maxmemory = 8ULL * 1024ULL * 1024ULL * 1024ULL;
    const uint64_t used_memory =
        1800ULL * 1024ULL * 1024ULL + static_cast<uint64_t>(worker_id) * 96ULL * 1024ULL * 1024ULL +
        static_cast<uint64_t>(round) * 16ULL * 1024ULL * 1024ULL;
    const uint64_t command_base = 2000000ULL + static_cast<uint64_t>(worker_id) * 70000ULL;
    const uint64_t command_delta = static_cast<uint64_t>(round) * 1500ULL;
    const uint64_t hits = command_base / 2ULL + command_delta * 85ULL / 100ULL;
    const uint64_t misses = command_base / 20ULL + command_delta * 8ULL / 100ULL;
    const double hit_percent = static_cast<double>(hits) / static_cast<double>(hits + misses) * 100.0;

    redis->set_instance(host + ":6379");
    redis->set_host(host);
    redis->set_port(6379);
    redis->set_up(true);
    redis->set_version("7.2.4");
    redis->set_role(worker_id % 4 == 0 ? "slave" : "master");
    redis->set_uptime_in_seconds(86400 + static_cast<uint64_t>(round) * 2ULL);
    redis->set_connected_clients(120 + worker_id * 5 + round % 30);
    redis->set_blocked_clients(worker_id % 6 == 0 && round % 5 == 0 ? 1 : 0);
    redis->set_maxclients(10000);
    redis->set_used_memory(used_memory);
    redis->set_maxmemory(maxmemory);
    redis->set_mem_fragmentation_ratio(1.08 + nextFloat(rng, 0.0f, 0.18f));
    redis->set_memory_used_percent(static_cast<double>(used_memory) / static_cast<double>(maxmemory) * 100.0);
    redis->set_total_commands_processed(command_base + command_delta);
    redis->set_instantaneous_ops_per_sec(800.0 + worker_id * 18.0 + nextFloat(rng, 0.0f, 120.0f));
    redis->set_keyspace_hits(hits);
    redis->set_keyspace_misses(misses);
    redis->set_keyspace_hit_percent(hit_percent);
    redis->set_expired_keys(4000 + static_cast<uint64_t>(worker_id) * 20ULL + static_cast<uint64_t>(round) * 6ULL);
    redis->set_evicted_keys(static_cast<uint64_t>(worker_id % 3) * static_cast<uint64_t>(round / 10));
    redis->set_rejected_connections(static_cast<uint64_t>(worker_id % 7 == 0 ? round / 30 : 0));
    redis->set_total_error_replies(static_cast<uint64_t>(worker_id % 5 == 0 ? round / 8 : 0));
    redis->set_total_net_input_bytes(command_base * 128ULL + command_delta * 180ULL);
    redis->set_total_net_output_bytes(command_base * 256ULL + command_delta * 320ULL);
    redis->set_replication_configured(worker_id % 4 == 0);
    redis->set_master_link_up(worker_id % 4 == 0);
    redis->set_connected_slaves(worker_id % 4 == 0 ? 0 : 1 + worker_id % 2);
    redis->set_master_last_io_seconds_ago(worker_id % 4 == 0 ? nextFloat(rng, 0.0f, 3.0f) : 0.0);
    redis->set_slowlog_len(5 + static_cast<uint64_t>(worker_id) + static_cast<uint64_t>(round / 6));
}

// 根据 worker_id 和 round 生成一个
// MonitorInfo，内容会有一定规律但又不完全相同，以便测试不同数据的推送和处理
monitor::proto::MonitorInfo makeMonitorInfo(int worker_id, int round) {
    std::mt19937 rng(static_cast<unsigned int>(worker_id * 1000 + round));
    monitor::proto::MonitorInfo info;
    const std::string hostname = workerName(worker_id);

    info.set_name(hostname);
    auto *host = info.mutable_host_info();
    host->set_hostname(hostname);
    host->set_ip_address(workerIp(worker_id));

    fillCpuStat(&info, rng, worker_id, round);
    fillCpuLoad(&info, rng, worker_id, round);
    fillMemory(&info, rng, worker_id, round);
    fillNet(&info, rng, worker_id, round);
    fillDisk(&info, rng, worker_id, round);
    fillSoftIrq(&info, rng);
    fillMysql(&info, rng, worker_id, round);
    fillRedis(&info, rng, worker_id, round);

    return info;
}

// 执行一次 push，返回是否成功
bool pushOnce(monitor::proto::GrpcManager::Stub *stub, int worker_id, int round) {
    grpc::ClientContext context;
    google::protobuf::Empty response;
    monitor::proto::MonitorInfo info = makeMonitorInfo(worker_id, round);
    grpc::Status status = stub->SetMonitorInfo(&context, info, &response);

    if (!status.ok()) {
        std::cerr << "[" << workerName(worker_id) << "] round " << round << " push failed: " << status.error_message()
                  << std::endl;
        return false;
    }

    std::cout << "[" << workerName(worker_id) << "] round " << round << " pushed as " << info.host_info().ip_address()
              << std::endl;
    return true;
}

void runWorker(const WorkerConfig &config, std::atomic<int> *success_count, std::atomic<int> *failure_count) {
    auto channel = grpc::CreateChannel(config.manager_address, grpc::InsecureChannelCredentials());
    auto stub = monitor::proto::GrpcManager::NewStub(channel);

    for (int round = 1; config.rounds == 0 || round <= config.rounds; ++round) {
        if (pushOnce(stub.get(), config.id, round))
            ++(*success_count);
        else
            ++(*failure_count);

        if (config.rounds != 0 && round == config.rounds) break;
        std::this_thread::sleep_for(std::chrono::seconds(config.interval_seconds));
    }
}

// 解析一个正整数，失败时返回回退默认值
int parsePositiveInt(const char *value, int fallback) {
    char *end = nullptr;
    long parsed = std::strtol(value, &end, 10);
    if (end == value || *end != '\0' || parsed <= 0) return fallback;
    return static_cast<int>(parsed);
}

// 解析一个非负整数，失败时返回回退默认值
int parseNonNegativeInt(const char *value, int fallback) {
    char *end = nullptr;
    long parsed = std::strtol(value, &end, 10);
    if (end == value || *end != '\0' || parsed < 0) return fallback;
    return static_cast<int>(parsed);
}

void printUsage(const char *program) {
    std::cout << "Usage: " << program << " [manager_address] [worker_count] [interval_seconds] [rounds]\n"
              << "Defaults: localhost:50051 5 2 5\n"
              << "Set rounds to 0 to push forever.\n";
}

} // namespace

int main(int argc, char *argv[]) {
    if (argc > 1 && std::string(argv[1]) == "--help") {
        printUsage(argv[0]);
        return 0;
    }

    const std::string manager_address = argc > 1 ? argv[1] : "localhost:50051";
    const int worker_count = argc > 2 ? parsePositiveInt(argv[2], 5) : 5;
    const int interval_seconds = argc > 3 ? parsePositiveInt(argv[3], 2) : 2;
    const int rounds = argc > 4 ? parseNonNegativeInt(argv[4], 5) : 5;

    std::cout << "Starting simulated workers\n"
              << "  manager: " << manager_address << "\n"
              << "  workers: " << worker_count << "\n"
              << "  interval: " << interval_seconds << "s\n"
              << "  rounds: " << (rounds == 0 ? std::string("forever") : std::to_string(rounds)) << std::endl;

    std::atomic<int> success_count{0};
    std::atomic<int> failure_count{0};
    std::vector<std::thread> workers;
    workers.reserve(static_cast<size_t>(worker_count));

    for (int id = 1; id <= worker_count; ++id) {
        WorkerConfig config{id, manager_address, interval_seconds, rounds};
        workers.emplace_back(runWorker, config, &success_count, &failure_count);
    }

    for (auto &worker : workers) worker.join();

    std::cout << "Finished simulated push: success=" << success_count.load() << ", failure=" << failure_count.load()
              << std::endl;
    return failure_count.load() == 0 ? 0 : 1;
}
