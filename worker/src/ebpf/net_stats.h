#ifndef __NETSTAT_H__
#define __NETSTAT_H__

#ifdef __cplusplus
extern "C"
{
#endif
#include <stdint.h>

#define NET_STATS_MAP_NAME "net_stats_map" // map name in ebpf
#define MAX_NET_DEVICES    64              // max network devices

    /**
     * @brief         net statistics, this is using for ebpf
     *
     */
    struct net_stat
    {
        uint64_t snd_bytes;
        uint64_t rcv_bytes;
        uint64_t snd_packets;
        uint64_t rcv_packets;
    };

#ifdef __cplusplus
}
#endif

#endif