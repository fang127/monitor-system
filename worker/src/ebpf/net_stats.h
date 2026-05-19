#ifndef __NETSTAT_H__
#define __NETSTAT_H__

#ifdef __cplusplus
extern "C" {
#endif
#include <stdint.h>

#define NET_STATS_MAP_NAME "net_stats_map" // eBPF map 名称
#define MAX_NET_DEVICES    64              // 最大网卡数量

/**
 * @brief         网络统计结构体，供 eBPF 程序和用户态共享
 *
 */
struct net_stat {
    uint64_t snd_bytes;
    uint64_t rcv_bytes;
    uint64_t snd_packets;
    uint64_t rcv_packets;
};

#ifdef __cplusplus
}
#endif

#endif
