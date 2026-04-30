/**
 * @brief         main function for worker
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
constexpr int kDefaultPushInterval = 10; // seconds

int main(int argc, char *argv[]) {
    std::string managerAddress = kDefaultManagerAddress;
    int intervalSeconds = kDefaultPushInterval;

    // parse command line arguments
    if (argc > 1) managerAddress = argv[1];
    if (argc > 2) {
        intervalSeconds = std::stoi(argv[2]);
        if (intervalSeconds <= 0) intervalSeconds = kDefaultPushInterval;
    }

    std::cout << "Starting Monitor Server (Push Mode) ..." << std::endl;
    std::cout << "Manager Address: " << managerAddress << std::endl;
    std::cout << "Push Interval: " << intervalSeconds << " seconds"
              << std::endl;

    // create and start the pusher
    monitor::MonitorPusher pusher(managerAddress, intervalSeconds);
    pusher.start();

    // wait for user input to stop
    std::cout << "Press Ctrl + C to exit." << std::endl;
    while (true) std::this_thread::sleep_for(std::chrono::seconds(60));

    return 0;
}