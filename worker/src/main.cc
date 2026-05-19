/**
 * @brief         worker 入口函数
 * @file          main.cc
 * @author        harry
 * @date          2026-02-11
 */

#include "MonitorPusher.h"

#include <chrono>
#include <iostream>
#include <string>
#include <thread>

constexpr char kDefaultManagerAddress[] = "localhost:50051";
constexpr int kDefaultPushInterval = 10; // 秒

int main(int argc, char *argv[]) {
    std::string managerAddress = kDefaultManagerAddress;
    int intervalSeconds = kDefaultPushInterval;

    // 解析命令行参数
    if (argc > 1) managerAddress = argv[1];
    if (argc > 2) {
        intervalSeconds = std::stoi(argv[2]);
        if (intervalSeconds <= 0) intervalSeconds = kDefaultPushInterval;
    }

    std::cout << "Starting Monitor Server (Push Mode) ..." << std::endl;
    std::cout << "Manager Address: " << managerAddress << std::endl;
    std::cout << "Push Interval: " << intervalSeconds << " seconds" << std::endl;

    // 创建并启动推送器
    monitor::MonitorPusher pusher(managerAddress, intervalSeconds);
    pusher.start();

    // 等待用户中断进程
    std::cout << "Press Ctrl + C to exit." << std::endl;
    while (true) std::this_thread::sleep_for(std::chrono::seconds(60));

    return 0;
}
