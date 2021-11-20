#include "io.h"


const size_t erspan_inner_offset = sizeof(struct rte_ether_hdr) + sizeof(struct rte_ipv4_hdr) + sizeof(struct ERSPAN);


/* 5-tuple key type */
struct flow_key
{
    uint32_t ip_src;
    uint32_t ip_dst;
    uint16_t port_src;
    uint16_t port_dst;
    uint8_t proto;
} __rte_cache_aligned;



static inline void
pkt_processing(struct rte_mbuf *pkt)
{
    struct ERSPAN *e;
    e = erspan_hdr(pkt);
    printf("seq: %6d | vlan: %5d | sess-id: %4d | src_id: %6d | dir: %2d | timestamp: %12ld |",
                _erspan_seq(e),_erspan_vlan(e),_erspan_sess_id(e),_erspan_src_id(e),_erspan_direction(e),_erspan_timestamp(e));

    /* inner data packet processing logic */
    struct rte_ipv4_hdr *ipv4_hdr;
    ipv4_hdr = (struct rte_ipv4_hdr *)(rte_pktmbuf_mtod(pkt, char *) + erspan_inner_offset + sizeof(struct rte_ether_hdr));
    printf("%d.%d.%d.%d -> %d.%d.%d.%d\n", NIPQUAD(ipv4_hdr->src_addr), NIPQUAD(ipv4_hdr->dst_addr));



    rte_pktmbuf_free(pkt);
    return;
}

int lcore_io(struct io_lcore_params *p)
{
    printf("Core %u doing packet RX.\n", rte_lcore_id());
    struct rte_mbuf *pkts[BURST_SIZE];
    struct rte_mbuf *ctrl_pkts[BURST_SIZE_TX];
    struct rte_eth_dev_tx_buffer *tx_buffer;

    // only thread 0 need to handle the packets from vhost interface.
    if (p->tid == 0)
    {
        // Initialize TX Buffer
        tx_buffer = rte_zmalloc_socket("tx_buffer",
                                       RTE_ETH_TX_BUFFER_SIZE(BURST_SIZE * 2), 0,
                                       rte_eth_dev_socket_id(ETH_PORT_ID));
        if (tx_buffer == NULL)
            rte_exit(EXIT_FAILURE, "Cannot allocate buffer for tx on port %u\n",
                     ETH_PORT_ID);

        int retval = rte_eth_tx_buffer_init(tx_buffer, BURST_SIZE * 2);
        if (retval < 0)
            rte_exit(EXIT_FAILURE,
                     "Cannot set error callback for tx buffer on port %u\n",
                     ETH_PORT_ID);
    }

    struct rte_ether_hdr *eth_hdr;
    struct rte_ipv4_hdr *ipv4_hdr;

    uint64_t prev_tsc, diff_tsc, cur_tsc, timer_tsc;
    uint16_t port_id;
    const uint64_t drain_tsc = (rte_get_tsc_hz() + US_PER_S - 1) / US_PER_S *
                               BURST_TX_DRAIN_US;
    prev_tsc = 0;
    timer_tsc = 0;

    while (!force_quit)
    {
        // process control packets from ctrl-ring
        if (unlikely(p->tid == 0))
        {
            cur_tsc = rte_rdtsc();
            diff_tsc = cur_tsc - prev_tsc;
            if (unlikely(diff_tsc > drain_tsc))
            {
                // drain tx_buffer
                rte_eth_tx_buffer_flush(ETH_PORT_ID, 0, tx_buffer);
                prev_tsc = cur_tsc;
            }

            // recieve pkts from control-ring and send to control interface
            const uint16_t nb_ctrl_rx = rte_ring_dequeue_burst(p->ctrl_ring,
                                                               (void *)ctrl_pkts, BURST_SIZE, NULL);
            unsigned int nb_ctrl_tx = rte_eth_tx_burst(CTRL_PORT_ID, 0, ctrl_pkts, nb_ctrl_rx);
            if (unlikely(nb_ctrl_tx < nb_ctrl_rx))
            {
                do
                {
                    rte_pktmbuf_free(ctrl_pkts[nb_ctrl_tx]);
                } while (++nb_ctrl_tx < nb_ctrl_rx);
            }

            // recieve pkts from control interface and send to Ethernet port
            const uint16_t nb_rx = rte_eth_rx_burst(CTRL_PORT_ID, 0, pkts,
                                                    BURST_SIZE);
            for (int i = 0; i < nb_rx; i++)
            {
                rte_eth_tx_buffer(ETH_PORT_ID, 0, tx_buffer, pkts[i]);
            }
        }

        const uint16_t nb_rx = rte_eth_rx_burst(ETH_PORT_ID, p->tid, pkts, BURST_SIZE);
        if (unlikely(nb_rx == 0))
        {
            continue;
        }

        int i;
        /* Prefetch first packets */
        for (i = 0; i < PREFETCH_OFFSET && i < nb_rx; i++)
        {
            rte_prefetch0(rte_pktmbuf_mtod(pkts[i], void *));
        }
        for (i = 0; i < (nb_rx - PREFETCH_OFFSET); i++)
        {
            rte_prefetch0(rte_pktmbuf_mtod(pkts[i + PREFETCH_OFFSET], void *));
            eth_hdr = rte_pktmbuf_mtod(pkts[i], struct rte_ether_hdr *);
            if (unlikely(eth_hdr->ether_type != rte_cpu_to_be_16(RTE_ETHER_TYPE_IPV4)))
            {
                rte_ring_enqueue(p->ctrl_ring, (void *)pkts[i]);
                continue;
            }
            ipv4_hdr = rte_pktmbuf_mtod_offset(pkts[i], struct rte_ipv4_hdr *, sizeof(struct rte_ether_hdr));
            if (unlikely(ipv4_hdr->next_proto_id != 0x2f))
            {
                rte_ring_enqueue(p->ctrl_ring, (void *)pkts[i]);
                continue;
            }
            pkt_processing(pkts[i]);
        }

        /* Process left packets */
        for (; i < nb_rx; i++)
        {
            eth_hdr = rte_pktmbuf_mtod(pkts[i], struct rte_ether_hdr *);
            if (unlikely(eth_hdr->ether_type != rte_cpu_to_be_16(RTE_ETHER_TYPE_IPV4)))
            {
                rte_ring_enqueue(p->ctrl_ring, (void *)pkts[i]);
                continue;
            }
            ipv4_hdr = rte_pktmbuf_mtod_offset(pkts[i], struct rte_ipv4_hdr *, sizeof(struct rte_ether_hdr));
            if (unlikely(ipv4_hdr->next_proto_id != 0x2f))
            {
                rte_ring_enqueue(p->ctrl_ring, (void *)pkts[i]);
                continue;
            }
            pkt_processing(pkts[i]);
        }
    }
    return 0;
}
