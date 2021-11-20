#ifndef __RUTA_COMMON_H_
#define __RUTA_COMMON_H_

#include <rte_eal.h>
#include <rte_ethdev.h>
#include <rte_cycles.h>
#include <rte_lcore.h>
#include <rte_malloc.h>
#include <rte_mbuf.h>
#include <rte_hash_crc.h>
#include <rte_bus_vdev.h>
#include <rte_ether.h>
#include <rte_cryptodev.h>
#include <rte_ip.h>
#include <rte_udp.h>
#include <inttypes.h>
#include <stdbool.h>
#include <signal.h>
#include <unistd.h>

#define RX_RING_SIZE 1024
#define TX_RING_SIZE 1024
#define NUM_MBUFS ((64 * 1024) - 1)
#define MBUF_CACHE_SIZE 128
#define SCHED_RX_RING_SZ 65536
#define SCHED_TX_RING_SZ 65536
#define ETH_MAX_FRAME_SIZE 9200

#define PREFETCH_OFFSET 4

#define ETH_PORT_ID 0
#define CTRL_PORT_ID 1

#define MAX_SERVICE_CORE 32

#define BURST_SIZE 32
#define BURST_SIZE_TX 64
#define BURST_TX_DRAIN_US 100

struct config
{
    int num_service_core;
    int first_lcore;
    char ip_addr[255];
    char netmask[255];
};

static struct config config;

static volatile bool force_quit;
static void
signal_handler(int s)
{
    if (s == SIGINT || s == SIGTERM)
    {
        printf("Signal %d received, preparing to exit...\n", s);
        force_quit = true;
    }
}

#define NIPQUAD(addr)                \
    ((unsigned char *)&addr)[0],     \
        ((unsigned char *)&addr)[1], \
        ((unsigned char *)&addr)[2], \
        ((unsigned char *)&addr)[3]


#endif /* __RUTA_COMMON_H_ */
