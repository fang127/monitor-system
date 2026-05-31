/**
 * @brief         worker 入口函数
 * @file          main.cc
 * @author        harry
 * @date          2026-02-11
 */

#include "MonitorPusher.h"

#include <cstdlib>
#include <chrono>
#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <csignal>

constexpr char kDefaultManagerAddress[] = "localhost:50051";
constexpr int kDefaultPushInterval = 10; // 秒

int main(int argc, char *argv[]) {
    std::string managerAddress = kDefaultManagerAddress;
    std::string workerID;
    std::string workerToken;
    int intervalSeconds = kDefaultPushInterval;

    // 解析命令行参数
    if (argc > 1) managerAddress = argv[1];
    if (argc > 2) {
        intervalSeconds = std::stoi(argv[2]);
        if (intervalSeconds <= 0) intervalSeconds = kDefaultPushInterval;
    }
    if (const char *envWorkerID = std::getenv("WORKER_ID")) workerID = envWorkerID;
    if (const char *envWorkerToken = std::getenv("WORKER_TOKEN")) workerToken = envWorkerToken;
    if (argc > 3) workerID = argv[3];
    if (argc > 4) workerToken = argv[4];

    std::cout << "Starting Monitor Server (Push Mode) ..." << std::endl;
    std::cout << "Manager Address: " << managerAddress << std::endl;
    std::cout << "Push Interval: " << intervalSeconds << " seconds" << std::endl;
    std::cout << "Worker ID: " << (workerID.empty() ? "<empty>" : workerID) << std::endl;

    // 创建并启动推送器
    std::unique_ptr<monitor::MonitorPusher> pusher =
        std::make_unique<monitor::MonitorPusher>(managerAddress, intervalSeconds, workerID, workerToken);
    pusher->start();

    // 等待用户中断进程
    std::cout << "Press Ctrl + C to exit." << std::endl;
    while (true) std::this_thread::sleep_for(std::chrono::seconds(60));

    return 0;
}
