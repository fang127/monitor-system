/**
 * @brief         test program for kmod cpu stat collector
 * @file          TestKmodCpuStat.cc
 * @author        harry
 * @date          2026-02-14
 */

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <stdint.h>
#include <string.h>

#define DEVICE_PATH "/dev/CpuStatCollector"
#define MAX_CPUS    256

/**
 * @brief         cpu stat structure - must match the one defined in the
 * kernel module
 *
 */
struct cpu_stat
{
    char cpu_name[16];
    uint64_t user;
    uint64_t nice;
    uint64_t system;
    uint64_t idle;
    uint64_t iowait;
    uint64_t irq;
    uint64_t softirq;
    uint64_t steal;
    uint64_t guest;
    uint64_t guest_nice;
};

int main(int argc, char *argv[])
{
    int fd;
    void *addr;
    struct cpu_stat *stats;
    size_t dataSize;
    int i;
    int count = 5;
    int interval = 1;

    if (argc > 1) count = atoi(argv[1]);
    if (argc > 2) interval = atoi(argv[2]);

    printf("=== CPU Stat Monitor Test ===\n");
    printf("Collecting %d samples with %d second interval\n\n", count,
           interval);

    /* 打开设备 */
    fd = open(DEVICE_PATH, O_RDONLY);
    if (fd < 0)
    {
        perror("Failed to open device");
        printf("Make sure the kernel module is loaded:\n");
        printf("  sudo insmod CpuStatCollector.ko\n");
        return 1;
    }

    dataSize = sizeof(struct cpu_stat) * MAX_CPUS;

    addr = mmap(NULL, dataSize, PROT_READ, MAP_SHARED, fd, 0);
    if (addr == MAP_FAILED)
    {
        perror("Failed to mmap");
        close(fd);
        return 1;
    }

    stats = (struct cpu_stat *)addr;

    for (int sample = 0; sample < count; sample++)
    {
        printf("--- Sample %d ---\n", sample + 1);
        printf("%-8s %12s %12s %12s %12s %12s %12s %12s %12s\n", "CPU", "user",
               "nice", "system", "idle", "iowait", "irq", "softirq", "steal");
        printf(
            "------------------------------------------------------------------"
            "--------------\n");

        for (i = 0; i < MAX_CPUS; i++)
        {
            if (stats[i].cpu_name[0] == '\0') break;

            printf("%-8s %12lu %12lu %12lu %12lu %12lu %12lu %12lu %12lu\n",
                   stats[i].cpu_name, (unsigned long)stats[i].user,
                   (unsigned long)stats[i].nice, (unsigned long)stats[i].system,
                   (unsigned long)stats[i].idle, (unsigned long)stats[i].iowait,
                   (unsigned long)stats[i].irq, (unsigned long)stats[i].softirq,
                   (unsigned long)stats[i].steal);
        }

        printf("\nTotal CPUs: %d\n\n", i);

        if (sample < count - 1) sleep(interval);
    }

    munmap(addr, dataSize);
    close(fd);

    printf("Test completed.\n");
    return 0;
}