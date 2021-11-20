#include <stdint.h>
#include <unistd.h>
#include <inttypes.h>

#include <pthread.h>
#include <string.h>

#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <net/if.h>
#include <net/if_arp.h>
#include <arpa/inet.h>

#include "portinit.h"
#include "io.h"
#include "cli_parser.h"



int main(int argc, char *argv[])
{
    struct rte_mempool *mbuf_pool;

    config.first_lcore = 24;
    config.num_service_core = 8;
    sprintf(config.ip_addr,"11.11.11.11");
    sprintf(config.netmask,"255.255.255.0");
    struct rte_ring *ctrl_ring;
    struct io_lcore_params lp[MAX_SERVICE_CORE];

    int retval = rte_eal_init(argc, argv);
    if (retval < 0)
        rte_exit(EXIT_FAILURE, "initialize fail!");

    retval = zspan_args_parser(argc, argv,&config);
    if (retval < 0)
        rte_exit(EXIT_FAILURE, "Invalid arguments\n");
    

    /* Creates a new mempool in memory to hold the mbufs. */
    mbuf_pool = rte_pktmbuf_pool_create("MBUF_POOL", NUM_MBUFS * 4,
                                        MBUF_CACHE_SIZE, 8,
                                        RTE_MBUF_DEFAULT_BUF_SIZE, rte_socket_id());
    if (mbuf_pool == NULL)
        rte_exit(EXIT_FAILURE, "Cannot create mbuf pool\n");

    /* Create Ctrl queue */
    ctrl_ring = rte_ring_create("ctrl_ring", SCHED_RX_RING_SZ,
                                   rte_socket_id(), RING_F_MC_HTS_DEQ | RING_F_MP_HTS_ENQ);
    if (ctrl_ring == NULL)
        rte_exit(EXIT_FAILURE, "Cannot create ctrl ring\n");

    struct rte_ether_addr eth_mac_addr;
    retval = rte_eth_macaddr_get(ETH_PORT_ID, &eth_mac_addr);

    /* Create control virtual port */
    char vhost_user_cfg[255];
    sprintf(vhost_user_cfg, "iface=rutasys0,path=/dev/vhost-net,queues=1,queue_size=1024,mac=%02"PRIx8 ":%02" PRIx8 ":%02" PRIx8 ":%02" PRIx8 ":%02" PRIx8 ":%02" PRIx8"\n",
            eth_mac_addr.addr_bytes[0], eth_mac_addr.addr_bytes[1],
            eth_mac_addr.addr_bytes[2], eth_mac_addr.addr_bytes[3],
            eth_mac_addr.addr_bytes[4], eth_mac_addr.addr_bytes[5]);

    printf("%s\n", vhost_user_cfg);
    rte_vdev_init("virtio_user0", vhost_user_cfg);

    if (port_init(CTRL_PORT_ID, mbuf_pool, 1, 1) != 0)
        rte_exit(EXIT_FAILURE, "Cannot init port %" PRIu16 "\n", CTRL_PORT_ID);
   
    /* Initialize eth port */
    if (port_init(ETH_PORT_ID, mbuf_pool, config.num_service_core, 1) != 0)
        rte_exit(EXIT_FAILURE, "Cannot init port %" PRIu16 "\n", ETH_PORT_ID);
    rte_eth_dev_default_mac_addr_set(ETH_PORT_ID, &eth_mac_addr);

    // Config tap0 interface address and mac address
    struct ifreq ifr;
    const char *name = "rutasys0";
    int fd = socket(PF_INET, SOCK_DGRAM, IPPROTO_IP);

    strncpy(ifr.ifr_name, name, IFNAMSIZ);
    ifr.ifr_addr.sa_family = AF_INET;
    inet_pton(AF_INET, config.ip_addr, ifr.ifr_addr.sa_data + 2);
    ioctl(fd, SIOCSIFADDR, &ifr);

    inet_pton(AF_INET, config.netmask, ifr.ifr_addr.sa_data + 2);
    ioctl(fd, SIOCSIFNETMASK, &ifr);

    ifr.ifr_addr.sa_family = ARPHRD_ETHER;
    for (int i = 0; i < 6; ++i)
        ifr.ifr_hwaddr.sa_data[i] = eth_mac_addr.addr_bytes[i];
    ioctl(fd, SIOCSIFHWADDR, &ifr);

    ioctl(fd, SIOCGIFFLAGS, &ifr);
    strncpy(ifr.ifr_name, name, IFNAMSIZ);
    ifr.ifr_flags |= (IFF_UP | IFF_RUNNING);
    ioctl(fd, SIOCSIFFLAGS, &ifr);

    printf("system init finished, starting service process...\n");

    unsigned int lcore_num = config.first_lcore;
    /* Start IO-RX process */   
    for (int i = 0; i < config.num_service_core; ++i)
    {
        lp[i].ctrl_ring = ctrl_ring;
        lp[i].mem_pool = mbuf_pool;
        lp[i].tid = i;
        rte_eal_remote_launch((lcore_function_t *)lcore_io, &lp[i], lcore_num++);
    }

    rte_eal_wait_lcore(config.first_lcore);
    return 0;
}

