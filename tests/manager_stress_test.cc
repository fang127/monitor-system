#include <google/protobuf/empty.pb.h>
#include <google/protobuf/timestamp.pb.h>
#include <grpcpp/grpcpp.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "monitor_info.grpc.pb.h"
#include "monitor_info.pb.h"
#include "query_api.grpc.pb.h"
#include "query_api.pb.h"

namespace {

using Clock = std::chrono::steady_clock;

struct Options {
    std::string manager_address = "localhost:50051";
    std::string mode = "auto";
    std::string query_kind = "latest";
    std::string query_server = "stress-worker-0001";
    int worker_concurrency = 1;
    int query_concurrency = 1;
    int max_worker_concurrency = 256;
    int max_query_concurrency = 256;
    int duration_seconds = 10;
    int warmup_seconds = 1;
    int request_timeout_ms = 3000;
    int page_size = 100;
    int query_window_seconds = 3600;
    double min_success_rate = 0.99;
    double max_p95_ms = 2000.0;
};

struct SideStats {
    uint64_t success = 0;
    uint64_t failure = 0;
    std::vector<double> success_latencies_ms;
    std::map<int, uint64_t> grpc_codes;
};

struct RunStats {
    SideStats worker;
    SideStats query;
    double seconds = 0.0;
};

struct Verdict {
    bool passed = false;
    double worker_success_rate = 1.0;
    double query_success_rate = 1.0;
    double worker_p95_ms = 0.0;
    double query_p95_ms = 0.0;
};

struct RunWindow {
    Clock::time_point measure_from;
    Clock::time_point end_at;
};

std::string workerName(int id) {
    std::ostringstream os;
    os << "stress-worker-" << std::setw(4) << std::setfill('0') << id;
    return os.str();
}

std::string workerIp(int id) {
    return "10.250." + std::to_string((id / 200) % 200) + "." +
           std::to_string(10 + (id % 200));
}

bool parsePositiveInt(const std::string &value, int *out) {
    char *end = nullptr;
    long parsed = std::strtol(value.c_str(), &end, 10);
    if (end == value.c_str() || *end != '\0' || parsed <= 0) return false;
    *out = static_cast<int>(parsed);
    return true;
}

bool parseNonNegativeInt(const std::string &value, int *out) {
    char *end = nullptr;
    long parsed = std::strtol(value.c_str(), &end, 10);
    if (end == value.c_str() || *end != '\0' || parsed < 0) return false;
    *out = static_cast<int>(parsed);
    return true;
}

bool parseNonNegativeDouble(const std::string &value, double *out) {
    char *end = nullptr;
    double parsed = std::strtod(value.c_str(), &end);
    if (end == value.c_str() || *end != '\0' || parsed < 0.0) return false;
    *out = parsed;
    return true;
}

google::protobuf::Timestamp timestampFromSystem(
    std::chrono::system_clock::time_point tp) {
    const auto seconds =
        std::chrono::time_point_cast<std::chrono::seconds>(tp);
    const auto nanos =
        std::chrono::duration_cast<std::chrono::nanoseconds>(tp - seconds);
    google::protobuf::Timestamp ts;
    ts.set_seconds(seconds.time_since_epoch().count());
    ts.set_nanos(static_cast<int>(nanos.count()));
    return ts;
}

monitor::proto::MonitorInfo makeMonitorInfo(int worker_id, uint64_t sequence) {
    monitor::proto::MonitorInfo info;
    const std::string hostname = workerName(worker_id);
    const float cycle = static_cast<float>((sequence + worker_id) % 60);
    const float cpu_percent = std::min(95.0f, 20.0f + cycle * 0.7f);
    const float mem_percent = std::min(90.0f, 35.0f + cycle * 0.5f);

    info.set_name(hostname);
    auto *host = info.mutable_host_info();
    host->set_hostname(hostname);
    host->set_ip_address(workerIp(worker_id));

    auto *cpu = info.add_cpu_stat();
    cpu->set_cpu_name("cpu");
    cpu->set_cpu_percent(cpu_percent);
    cpu->set_usr_percent(cpu_percent * 0.55f);
    cpu->set_system_percent(cpu_percent * 0.25f);
    cpu->set_nice_percent(cpu_percent * 0.01f);
    cpu->set_idle_percent(100.0f - cpu_percent);
    cpu->set_io_wait_percent(cpu_percent * 0.12f);
    cpu->set_irq_percent(cpu_percent * 0.02f);
    cpu->set_soft_irq_percent(cpu_percent * 0.05f);

    auto *load = info.mutable_cpu_load();
    load->set_load_avg_1(0.4f + cycle * 0.03f);
    load->set_load_avg_3(0.35f + cycle * 0.025f);
    load->set_load_avg_15(0.3f + cycle * 0.02f);

    auto *mem = info.mutable_mem_info();
    const float total = 32768.0f;
    const float used = total * mem_percent / 100.0f;
    mem->set_total(total);
    mem->set_free(total - used);
    mem->set_avail(total - used * 0.75f);
    mem->set_buffers(512.0f);
    mem->set_cached(4096.0f);
    mem->set_swap_cached(0.0f);
    mem->set_active(used * 0.6f);
    mem->set_inactive(used * 0.2f);
    mem->set_active_anon(used * 0.3f);
    mem->set_inactive_anon(used * 0.1f);
    mem->set_active_file(used * 0.25f);
    mem->set_inactive_file(used * 0.1f);
    mem->set_dirty(64.0f);
    mem->set_writeback(4.0f);
    mem->set_anon_pages(used * 0.25f);
    mem->set_mapped(used * 0.08f);
    mem->set_kreclaimable(768.0f);
    mem->set_sreclaimable(512.0f);
    mem->set_sunreclaim(256.0f);
    mem->set_used_percent(mem_percent);

    auto *net = info.add_net_info();
    net->set_name("eth0");
    net->set_send_rate(600000.0f + cycle * 10000.0f);
    net->set_rcv_rate(900000.0f + cycle * 12000.0f);
    net->set_send_packets_rate(800.0f + cycle);
    net->set_rcv_packets_rate(1000.0f + cycle);

    auto *disk = info.add_disk_info();
    disk->set_name("sda");
    disk->set_reads(100000 + sequence);
    disk->set_writes(80000 + sequence);
    disk->set_sectors_read(2000000 + sequence * 8);
    disk->set_sectors_written(1500000 + sequence * 8);
    disk->set_read_time_ms(20000 + sequence);
    disk->set_write_time_ms(24000 + sequence);
    disk->set_io_in_progress(sequence % 3);
    disk->set_io_time_ms(30000 + sequence);
    disk->set_weighted_io_time_ms(45000 + sequence);
    disk->set_read_bytes_per_sec(2.0 * 1024 * 1024 + cycle * 1024);
    disk->set_write_bytes_per_sec(1.5 * 1024 * 1024 + cycle * 1024);
    disk->set_read_iops(120.0 + cycle);
    disk->set_write_iops(90.0 + cycle);
    disk->set_avg_read_latency_ms(1.2 + cycle * 0.01);
    disk->set_avg_write_latency_ms(1.5 + cycle * 0.01);
    disk->set_util_percent(std::min(90.0, 20.0 + cycle * 0.6));

    for (int core = 0; core < 4; ++core) {
        auto *soft_irq = info.add_soft_irq();
        soft_irq->set_cpu("cpu" + std::to_string(core));
        soft_irq->set_hi(cycle);
        soft_irq->set_timer(10.0f + cycle);
        soft_irq->set_net_tx(5.0f + cycle);
        soft_irq->set_net_rx(6.0f + cycle);
        soft_irq->set_block(1.0f + cycle * 0.1f);
        soft_irq->set_irq_poll(cycle * 0.1f);
        soft_irq->set_tasklet(cycle * 0.2f);
        soft_irq->set_sched(2.0f + cycle * 0.3f);
        soft_irq->set_hrtimer(cycle * 0.1f);
        soft_irq->set_rcu(1.0f + cycle * 0.2f);
    }

    return info;
}

void setDeadline(grpc::ClientContext *context, int timeout_ms) {
    context->set_deadline(std::chrono::system_clock::now() +
                          std::chrono::milliseconds(timeout_ms));
}

void fillTimeRange(monitor::proto::TimeRange *time_range,
                   int query_window_seconds) {
    const auto now = std::chrono::system_clock::now();
    *time_range->mutable_start_time() =
        timestampFromSystem(now - std::chrono::seconds(query_window_seconds));
    *time_range->mutable_end_time() = timestampFromSystem(now);
}

grpc::Status runQueryOnce(monitor::proto::QueryService::Stub *stub,
                          const Options &options) {
    grpc::ClientContext context;
    setDeadline(&context, options.request_timeout_ms);

    if (options.query_kind == "rank") {
        monitor::proto::QueryScoreRankRequest request;
        monitor::proto::QueryScoreRankResponse response;
        request.set_order(monitor::proto::DESC);
        request.mutable_pagination()->set_page(1);
        request.mutable_pagination()->set_page_size(options.page_size);
        return stub->QueryScoreRank(&context, request, &response);
    }

    if (options.query_kind == "performance") {
        monitor::proto::QueryPerformanceRequest request;
        monitor::proto::QueryPerformanceResponse response;
        request.set_server_name(options.query_server);
        fillTimeRange(request.mutable_time_range(), options.query_window_seconds);
        request.mutable_pagination()->set_page(1);
        request.mutable_pagination()->set_page_size(options.page_size);
        return stub->QueryPerformance(&context, request, &response);
    }

    monitor::proto::QueryLatestScoreRequest request;
    monitor::proto::QueryLatestScoreResponse response;
    return stub->QueryLatestScore(&context, request, &response);
}

double percentile(std::vector<double> values, double p) {
    if (values.empty()) return 0.0;
    std::sort(values.begin(), values.end());
    const double idx = (values.size() - 1) * p;
    const size_t lo = static_cast<size_t>(std::floor(idx));
    const size_t hi = static_cast<size_t>(std::ceil(idx));
    if (lo == hi) return values[lo];
    const double frac = idx - static_cast<double>(lo);
    return values[lo] * (1.0 - frac) + values[hi] * frac;
}

double successRate(const SideStats &stats) {
    const uint64_t total = stats.success + stats.failure;
    if (total == 0) return 1.0;
    return static_cast<double>(stats.success) / static_cast<double>(total);
}

double rps(const SideStats &stats, double seconds) {
    if (seconds <= 0.0) return 0.0;
    return static_cast<double>(stats.success + stats.failure) / seconds;
}

void mergeSideStats(SideStats *dst, const SideStats &src) {
    dst->success += src.success;
    dst->failure += src.failure;
    dst->success_latencies_ms.insert(dst->success_latencies_ms.end(),
                                     src.success_latencies_ms.begin(),
                                     src.success_latencies_ms.end());
    for (const auto &entry : src.grpc_codes) dst->grpc_codes[entry.first] += entry.second;
}

void workerLoop(const Options &options, int id, const RunWindow *window,
                std::atomic<int> *ready, std::atomic<bool> *start,
                SideStats *result) {
    auto channel =
        grpc::CreateChannel(options.manager_address, grpc::InsecureChannelCredentials());
    auto stub = monitor::proto::GrpcManager::NewStub(channel);
    ++(*ready);
    while (!start->load(std::memory_order_acquire)) std::this_thread::yield();

    uint64_t sequence = 0;
    while (Clock::now() < window->end_at) {
        grpc::ClientContext context;
        setDeadline(&context, options.request_timeout_ms);
        google::protobuf::Empty response;
        auto request = makeMonitorInfo(id, ++sequence);

        const auto begin = Clock::now();
        grpc::Status status = stub->SetMonitorInfo(&context, request, &response);
        const auto elapsed =
            std::chrono::duration<double, std::milli>(Clock::now() - begin).count();
        if (begin < window->measure_from) continue;

        if (status.ok()) {
            ++result->success;
            result->success_latencies_ms.push_back(elapsed);
        } else {
            ++result->failure;
            ++result->grpc_codes[static_cast<int>(status.error_code())];
        }
    }
}

void queryLoop(const Options &options, int id, const RunWindow *window,
               std::atomic<int> *ready, std::atomic<bool> *start,
               SideStats *result) {
    (void)id;
    auto channel =
        grpc::CreateChannel(options.manager_address, grpc::InsecureChannelCredentials());
    auto stub = monitor::proto::QueryService::NewStub(channel);
    ++(*ready);
    while (!start->load(std::memory_order_acquire)) std::this_thread::yield();

    while (Clock::now() < window->end_at) {
        const auto begin = Clock::now();
        grpc::Status status = runQueryOnce(stub.get(), options);
        const auto elapsed =
            std::chrono::duration<double, std::milli>(Clock::now() - begin).count();
        if (begin < window->measure_from) continue;

        if (status.ok()) {
            ++result->success;
            result->success_latencies_ms.push_back(elapsed);
        } else {
            ++result->failure;
            ++result->grpc_codes[static_cast<int>(status.error_code())];
        }
    }
}

RunStats runLoad(const Options &options, int worker_concurrency,
                 int query_concurrency, bool quiet) {
    if (!quiet) {
        std::cout << "\n[run] workers=" << worker_concurrency
                  << " queries=" << query_concurrency
                  << " duration=" << options.duration_seconds << "s"
                  << " query_kind=" << options.query_kind << std::endl;
    }

    const int total_threads = worker_concurrency + query_concurrency;
    std::atomic<int> ready{0};
    std::atomic<bool> start{false};
    std::vector<std::thread> threads;
    std::vector<SideStats> worker_results(worker_concurrency);
    std::vector<SideStats> query_results(query_concurrency);
    threads.reserve(static_cast<size_t>(total_threads));
    RunWindow window;

    for (int i = 0; i < worker_concurrency; ++i) {
        threads.emplace_back(workerLoop, std::cref(options), i + 1, &window,
                             &ready, &start, &worker_results[i]);
    }
    for (int i = 0; i < query_concurrency; ++i) {
        threads.emplace_back(queryLoop, std::cref(options), i + 1, &window,
                             &ready, &start, &query_results[i]);
    }

    while (ready.load(std::memory_order_acquire) < total_threads) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    const auto begin = Clock::now();
    window.measure_from = begin + std::chrono::seconds(options.warmup_seconds);
    window.end_at = window.measure_from + std::chrono::seconds(options.duration_seconds);
    start.store(true, std::memory_order_release);
    for (auto &thread : threads) thread.join();
    const double elapsed =
        std::chrono::duration<double>(Clock::now() - window.measure_from).count();

    RunStats stats;
    stats.seconds = elapsed;
    for (const auto &result : worker_results) mergeSideStats(&stats.worker, result);
    for (const auto &result : query_results) mergeSideStats(&stats.query, result);
    return stats;
}

Verdict judge(const Options &options, const RunStats &stats,
              int worker_concurrency, int query_concurrency) {
    Verdict verdict;
    verdict.worker_success_rate = successRate(stats.worker);
    verdict.query_success_rate = successRate(stats.query);
    verdict.worker_p95_ms = percentile(stats.worker.success_latencies_ms, 0.95);
    verdict.query_p95_ms = percentile(stats.query.success_latencies_ms, 0.95);

    const bool worker_has_requests =
        worker_concurrency == 0 || (stats.worker.success + stats.worker.failure) > 0;
    const bool query_has_requests =
        query_concurrency == 0 || (stats.query.success + stats.query.failure) > 0;
    const bool worker_ok =
        worker_concurrency == 0 ||
        (worker_has_requests && verdict.worker_success_rate >= options.min_success_rate &&
         (options.max_p95_ms <= 0.0 || verdict.worker_p95_ms <= options.max_p95_ms));
    const bool query_ok =
        query_concurrency == 0 ||
        (query_has_requests && verdict.query_success_rate >= options.min_success_rate &&
         (options.max_p95_ms <= 0.0 || verdict.query_p95_ms <= options.max_p95_ms));
    verdict.passed = worker_ok && query_ok;
    return verdict;
}

void printGrpcCodes(const std::map<int, uint64_t> &codes) {
    if (codes.empty()) return;
    std::cout << " codes={";
    bool first = true;
    for (const auto &entry : codes) {
        if (!first) std::cout << ",";
        first = false;
        std::cout << entry.first << ":" << entry.second;
    }
    std::cout << "}";
}

void printSide(const char *name, const SideStats &stats, double seconds) {
    const uint64_t total = stats.success + stats.failure;
    std::cout << "  " << name << ": total=" << total
              << " success=" << stats.success << " failure=" << stats.failure
              << " success_rate=" << std::fixed << std::setprecision(4)
              << successRate(stats)
              << " rps=" << std::setprecision(1) << rps(stats, seconds)
              << " p50_ms=" << std::setprecision(2)
              << percentile(stats.success_latencies_ms, 0.50)
              << " p95_ms=" << percentile(stats.success_latencies_ms, 0.95)
              << " p99_ms=" << percentile(stats.success_latencies_ms, 0.99);
    printGrpcCodes(stats.grpc_codes);
    std::cout << std::endl;
}

void printRunResult(const Options &options, const RunStats &stats,
                    int worker_concurrency, int query_concurrency) {
    const Verdict verdict =
        judge(options, stats, worker_concurrency, query_concurrency);
    std::cout << "  verdict=" << (verdict.passed ? "PASS" : "FAIL")
              << " elapsed=" << std::fixed << std::setprecision(2)
              << stats.seconds << "s" << std::endl;
    if (worker_concurrency > 0) printSide("worker", stats.worker, stats.seconds);
    if (query_concurrency > 0) printSide("query ", stats.query, stats.seconds);
}

bool runAndPrint(const Options &options, int worker_concurrency,
                 int query_concurrency) {
    RunStats stats = runLoad(options, worker_concurrency, query_concurrency, false);
    printRunResult(options, stats, worker_concurrency, query_concurrency);
    return judge(options, stats, worker_concurrency, query_concurrency).passed;
}

std::vector<int> buildRamp(int max_concurrency) {
    std::vector<int> values;
    for (int value = 1; value < max_concurrency; value *= 2) {
        values.push_back(value);
        if (value > max_concurrency / 2) break;
    }
    if (values.empty() || values.back() != max_concurrency)
        values.push_back(max_concurrency);
    return values;
}

int binaryRefine(const Options &options, int low_pass, int high_fail,
                 bool worker_side) {
    int best = low_pass;
    int left = low_pass + 1;
    int right = high_fail - 1;
    while (left <= right) {
        const int mid = left + (right - left) / 2;
        const bool passed =
            worker_side ? runAndPrint(options, mid, 0) : runAndPrint(options, 0, mid);
        if (passed) {
            best = mid;
            left = mid + 1;
        } else {
            right = mid - 1;
        }
    }
    return best;
}

int findMaxSingle(const Options &options, bool worker_side) {
    const int cap =
        worker_side ? options.max_worker_concurrency : options.max_query_concurrency;
    int best = 0;
    int previous = 0;
    for (int value : buildRamp(cap)) {
        const bool passed =
            worker_side ? runAndPrint(options, value, 0) : runAndPrint(options, 0, value);
        if (passed) {
            best = value;
            previous = value;
        } else {
            return binaryRefine(options, previous, value, worker_side);
        }
    }
    return best;
}

std::pair<int, int> scaledPair(int worker_max, int query_max, double scale) {
    if (scale <= 0.0) return {0, 0};
    const int worker = std::max(1, static_cast<int>(std::floor(worker_max * scale)));
    const int query = std::max(1, static_cast<int>(std::floor(query_max * scale)));
    return {worker, query};
}

std::pair<int, int> findMaxMixed(const Options &options, int worker_max,
                                 int query_max) {
    if (worker_max <= 0 || query_max <= 0) return {0, 0};

    double best_scale = 0.0;
    double left = 0.05;
    double right = 1.0;
    for (int i = 0; i < 8; ++i) {
        const double mid = (left + right) / 2.0;
        const auto pair = scaledPair(worker_max, query_max, mid);
        const bool passed = runAndPrint(options, pair.first, pair.second);
        if (passed) {
            best_scale = mid;
            left = mid;
        } else {
            right = mid;
        }
    }
    return scaledPair(worker_max, query_max, best_scale);
}

void printUsage(const char *program) {
    std::cout
        << "Usage: " << program << " [options]\n"
        << "\n"
        << "Modes:\n"
        << "  --mode auto|worker|query|mixed   default: auto\n"
        << "\n"
        << "Connection and request options:\n"
        << "  --manager ADDRESS                default: localhost:50051\n"
        << "  --query-kind latest|rank|performance   default: latest\n"
        << "  --query-server NAME              default: stress-worker-0001\n"
        << "  --duration-seconds N             default: 10\n"
        << "  --warmup-seconds N               default: 1\n"
        << "  --request-timeout-ms N           default: 3000\n"
        << "  --page-size N                    default: 100\n"
        << "  --query-window-seconds N         default: 3600\n"
        << "\n"
        << "Concurrency options:\n"
        << "  --worker-concurrency N           used by worker/mixed mode\n"
        << "  --query-concurrency N            used by query/mixed mode\n"
        << "  --max-worker-concurrency N       auto search cap, default: 256\n"
        << "  --max-query-concurrency N        auto search cap, default: 256\n"
        << "\n"
        << "Pass criteria:\n"
        << "  --min-success-rate FLOAT         default: 0.99\n"
        << "  --max-p95-ms FLOAT               default: 2000, 0 disables p95 limit\n";
}

bool parseArgs(int argc, char **argv, Options *options) {
    for (int i = 1; i < argc; ++i) {
        const std::string key = argv[i];
        auto requireValue = [&](std::string *value) {
            if (i + 1 >= argc) {
                std::cerr << "Missing value for " << key << std::endl;
                return false;
            }
            *value = argv[++i];
            return true;
        };
        std::string value;
        if (key == "--help" || key == "-h") {
            printUsage(argv[0]);
            std::exit(0);
        } else if (key == "--manager") {
            if (!requireValue(&options->manager_address)) return false;
        } else if (key == "--mode") {
            if (!requireValue(&options->mode)) return false;
        } else if (key == "--query-kind") {
            if (!requireValue(&options->query_kind)) return false;
        } else if (key == "--query-server") {
            if (!requireValue(&options->query_server)) return false;
        } else if (key == "--worker-concurrency") {
            if (!requireValue(&value) ||
                !parsePositiveInt(value, &options->worker_concurrency))
                return false;
        } else if (key == "--query-concurrency") {
            if (!requireValue(&value) ||
                !parsePositiveInt(value, &options->query_concurrency))
                return false;
        } else if (key == "--max-worker-concurrency") {
            if (!requireValue(&value) ||
                !parsePositiveInt(value, &options->max_worker_concurrency))
                return false;
        } else if (key == "--max-query-concurrency") {
            if (!requireValue(&value) ||
                !parsePositiveInt(value, &options->max_query_concurrency))
                return false;
        } else if (key == "--duration-seconds") {
            if (!requireValue(&value) ||
                !parsePositiveInt(value, &options->duration_seconds))
                return false;
        } else if (key == "--warmup-seconds") {
            if (!requireValue(&value) ||
                !parseNonNegativeInt(value, &options->warmup_seconds))
                return false;
        } else if (key == "--request-timeout-ms") {
            if (!requireValue(&value) ||
                !parsePositiveInt(value, &options->request_timeout_ms))
                return false;
        } else if (key == "--page-size") {
            if (!requireValue(&value) || !parsePositiveInt(value, &options->page_size))
                return false;
        } else if (key == "--query-window-seconds") {
            if (!requireValue(&value) ||
                !parsePositiveInt(value, &options->query_window_seconds))
                return false;
        } else if (key == "--min-success-rate") {
            if (!requireValue(&value) ||
                !parseNonNegativeDouble(value, &options->min_success_rate))
                return false;
        } else if (key == "--max-p95-ms") {
            if (!requireValue(&value) ||
                !parseNonNegativeDouble(value, &options->max_p95_ms))
                return false;
        } else {
            std::cerr << "Unknown option: " << key << std::endl;
            return false;
        }
    }

    if (options->mode != "auto" && options->mode != "worker" &&
        options->mode != "query" && options->mode != "mixed") {
        std::cerr << "Invalid --mode: " << options->mode << std::endl;
        return false;
    }
    if (options->query_kind != "latest" && options->query_kind != "rank" &&
        options->query_kind != "performance") {
        std::cerr << "Invalid --query-kind: " << options->query_kind << std::endl;
        return false;
    }
    if (options->min_success_rate > 1.0) {
        std::cerr << "--min-success-rate must be <= 1.0" << std::endl;
        return false;
    }
    return true;
}

void printConfig(const Options &options) {
    std::cout << "Manager stress test\n"
              << "  manager=" << options.manager_address << "\n"
              << "  mode=" << options.mode << "\n"
              << "  query_kind=" << options.query_kind << "\n"
              << "  duration=" << options.duration_seconds << "s"
              << " warmup=" << options.warmup_seconds << "s"
              << " timeout=" << options.request_timeout_ms << "ms\n"
              << "  pass: success_rate>=" << options.min_success_rate
              << " p95<="
              << (options.max_p95_ms <= 0.0 ? std::string("disabled")
                                            : std::to_string(options.max_p95_ms) + "ms")
              << std::endl;
}

} // namespace

int main(int argc, char **argv) {
    Options options;
    if (!parseArgs(argc, argv, &options)) {
        printUsage(argv[0]);
        return 2;
    }

    printConfig(options);

    if (options.mode == "worker") {
        return runAndPrint(options, options.worker_concurrency, 0) ? 0 : 1;
    }
    if (options.mode == "query") {
        return runAndPrint(options, 0, options.query_concurrency) ? 0 : 1;
    }
    if (options.mode == "mixed") {
        return runAndPrint(options, options.worker_concurrency,
                           options.query_concurrency)
                   ? 0
                   : 1;
    }

    std::cout << "\n=== worker-only max concurrency ===" << std::endl;
    const int worker_max = findMaxSingle(options, true);

    std::cout << "\n=== query-only max concurrency ===" << std::endl;
    const int query_max = findMaxSingle(options, false);

    std::cout << "\n=== mixed max concurrency ===" << std::endl;
    const auto mixed_max = findMaxMixed(options, worker_max, query_max);

    std::cout << "\nSummary\n"
              << "  worker_only_max_concurrency=" << worker_max << "\n"
              << "  query_only_max_concurrency=" << query_max << "\n"
              << "  mixed_max_worker_concurrency=" << mixed_max.first << "\n"
              << "  mixed_max_query_concurrency=" << mixed_max.second << "\n"
              << "  mixed_max_total_concurrency="
              << (mixed_max.first + mixed_max.second) << std::endl;

    return worker_max > 0 && query_max > 0 &&
                   mixed_max.first > 0 && mixed_max.second > 0
               ? 0
               : 1;
}
