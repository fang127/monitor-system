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

float nextFloat(std::mt19937 &rng, float min, float max) {
    std::uniform_real_distribution<float> dist(min, max);
    return dist(rng);
}

void fillCpuStat(monitor::proto::MonitorInfo *info, std::mt19937 &rng,
                 int worker_id, int round) {
    auto add_cpu = [&](const std::string &name, float base) {
        auto *cpu = info->add_cpu_stat();
        const float cpu_percent =
            std::min(98.0f, std::max(1.0f, base + nextFloat(rng, -5.0f, 5.0f)));
        const float usr = cpu_percent * nextFloat(rng, 0.45f, 0.60f);
        const float system = cpu_percent * nextFloat(rng, 0.20f, 0.35f);
        const float nice = cpu_percent * nextFloat(rng, 0.00f, 0.03f);
        const float irq = cpu_percent * nextFloat(rng, 0.00f, 0.02f);
        const float soft_irq = cpu_percent * nextFloat(rng, 0.01f, 0.04f);
        const float io_wait =
            std::max(0.0f, cpu_percent - usr - system - nice - irq - soft_irq);

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

    const float base = 18.0f + worker_id * 8.0f + (round % 4) * 3.0f;
    add_cpu("cpu", base);
    for (int core = 0; core < 4; ++core)
        add_cpu("cpu" + std::to_string(core), base + core * 1.5f);
}

void fillCpuLoad(monitor::proto::MonitorInfo *info, std::mt19937 &rng,
                 int worker_id, int round) {
    auto *load = info->mutable_cpu_load();
    const float base = 0.4f + worker_id * 0.25f + (round % 3) * 0.15f;
    load->set_load_avg_1(base + nextFloat(rng, 0.0f, 0.25f));
    load->set_load_avg_3(base + nextFloat(rng, 0.0f, 0.20f));
    load->set_load_avg_15(base + nextFloat(rng, 0.0f, 0.15f));
}

void fillMemory(monitor::proto::MonitorInfo *info, std::mt19937 &rng,
                int worker_id, int round) {
    auto *mem = info->mutable_mem_info();
    const float total = 32768.0f;
    const float used_percent =
        std::min(92.0f, 25.0f + worker_id * 8.0f + round * 1.2f +
                            nextFloat(rng, -2.0f, 2.0f));
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

void fillNet(monitor::proto::MonitorInfo *info, std::mt19937 &rng,
             int worker_id, int round) {
    auto *net = info->add_net_info();
    net->set_name("eth0");
    net->set_send_rate(600.0f + worker_id * 95.0f + round * 20.0f +
                       nextFloat(rng, 0.0f, 80.0f));
    net->set_rcv_rate(900.0f + worker_id * 130.0f + round * 35.0f +
                      nextFloat(rng, 0.0f, 120.0f));
    net->set_send_packets_rate(500.0f + worker_id * 20.0f +
                               nextFloat(rng, 0.0f, 50.0f));
    net->set_rcv_packets_rate(700.0f + worker_id * 30.0f +
                              nextFloat(rng, 0.0f, 70.0f));
    net->set_err_in(worker_id == 5 && round % 4 == 0 ? 1 : 0);
    net->set_err_out(0);
    net->set_drop_in(worker_id == 4 && round % 5 == 0 ? 2 : 0);
    net->set_drop_out(0);
}

void fillDisk(monitor::proto::MonitorInfo *info, std::mt19937 &rng,
              int worker_id, int round) {
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
    disk->set_read_bytes_per_sec(2.0 * 1024 * 1024 + worker_id * 128 * 1024 +
                                 nextFloat(rng, 0.0f, 256.0f * 1024));
    disk->set_write_bytes_per_sec(1.5 * 1024 * 1024 + worker_id * 96 * 1024 +
                                  nextFloat(rng, 0.0f, 200.0f * 1024));
    disk->set_read_iops(100.0 + worker_id * 8.0 + nextFloat(rng, 0.0f, 20.0f));
    disk->set_write_iops(80.0 + worker_id * 7.0 + nextFloat(rng, 0.0f, 15.0f));
    disk->set_avg_read_latency_ms(1.0 + worker_id * 0.2 +
                                  nextFloat(rng, 0.0f, 0.8f));
    disk->set_avg_write_latency_ms(1.4 + worker_id * 0.25 +
                                   nextFloat(rng, 0.0f, 1.0f));
    disk->set_util_percent(std::min(95.0, 10.0 + worker_id * 7.0 + round * 1.5 +
                                              nextFloat(rng, 0.0f, 6.0f)));
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

    return info;
}

bool pushOnce(monitor::proto::GrpcManager::Stub *stub, int worker_id,
              int round) {
    grpc::ClientContext context;
    google::protobuf::Empty response;
    monitor::proto::MonitorInfo info = makeMonitorInfo(worker_id, round);
    grpc::Status status = stub->SetMonitorInfo(&context, info, &response);

    if (!status.ok()) {
        std::cerr << "[" << workerName(worker_id) << "] round " << round
                  << " push failed: " << status.error_message() << std::endl;
        return false;
    }

    std::cout << "[" << workerName(worker_id) << "] round " << round
              << " pushed as " << info.host_info().ip_address() << std::endl;
    return true;
}

void runWorker(const WorkerConfig &config, std::atomic<int> *success_count,
               std::atomic<int> *failure_count) {
    auto channel = grpc::CreateChannel(config.manager_address,
                                       grpc::InsecureChannelCredentials());
    auto stub = monitor::proto::GrpcManager::NewStub(channel);

    for (int round = 1; config.rounds == 0 || round <= config.rounds; ++round) {
        if (pushOnce(stub.get(), config.id, round))
            ++(*success_count);
        else
            ++(*failure_count);

        if (config.rounds != 0 && round == config.rounds) break;
        std::this_thread::sleep_for(
            std::chrono::seconds(config.interval_seconds));
    }
}

int parsePositiveInt(const char *value, int fallback) {
    char *end = nullptr;
    long parsed = std::strtol(value, &end, 10);
    if (end == value || *end != '\0' || parsed <= 0) return fallback;
    return static_cast<int>(parsed);
}

int parseNonNegativeInt(const char *value, int fallback) {
    char *end = nullptr;
    long parsed = std::strtol(value, &end, 10);
    if (end == value || *end != '\0' || parsed < 0) return fallback;
    return static_cast<int>(parsed);
}

void printUsage(const char *program) {
    std::cout
        << "Usage: " << program
        << " [manager_address] [worker_count] [interval_seconds] [rounds]\n"
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
              << "  rounds: "
              << (rounds == 0 ? std::string("forever") : std::to_string(rounds))
              << std::endl;

    std::atomic<int> success_count{0};
    std::atomic<int> failure_count{0};
    std::vector<std::thread> workers;
    workers.reserve(static_cast<size_t>(worker_count));

    for (int id = 1; id <= worker_count; ++id) {
        WorkerConfig config{id, manager_address, interval_seconds, rounds};
        workers.emplace_back(runWorker, config, &success_count, &failure_count);
    }

    for (auto &worker : workers) worker.join();

    std::cout << "Finished simulated push: success=" << success_count.load()
              << ", failure=" << failure_count.load() << std::endl;
    return failure_count.load() == 0 ? 0 : 1;
}
