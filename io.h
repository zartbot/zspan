#include "common.h"

struct io_lcore_params
{
    struct rte_ring *ctrl_ring;
    struct rte_mempool *mem_pool;
    uint16_t tid;    
};

int lcore_io(struct io_lcore_params *p);


/* Nexus 9000 ERSPAN Type-3 Header

GRE Header:

    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    |C|R|K|S|s|Recur|  Flags  | Ver |         Protocol Type         |
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    |                            Sequence                           |
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

ERSPAN Type III header:

    0                   1                   2                   3
    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |  Ver  |          VLAN         | COS |BSO|T|     Session ID    |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |                          Timestamp                            |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |             SGT               |P|    FT   |   Hw ID   |D|Gra|O|
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

Optional Header:

    0                   1                   2                   3 
    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+ 
   |0x7 or 0x0 |  Reserved |     Source-Index(SI)                  | 
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+ 
   |                    Timestamp(MSB)                             | 
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+ 
*/


struct ERSPAN 
{
    uint16_t gre_flag;
    uint16_t gre_type;
    uint32_t gre_seq;
    uint16_t _ver_vlan;
    uint16_t _flag1;
    uint32_t timestamp;
    uint16_t sgt;
    uint8_t _temp;
    uint8_t flag2;
    uint32_t option1;
    uint32_t timestamp_msb;
} __attribute__ ((packed));
    
static inline uint32_t _erspan_seq(struct ERSPAN *e) {
    return rte_be_to_cpu_32(e->gre_seq) ;
}

static inline uint16_t _erspan_vlan(struct ERSPAN *e) {
    return rte_be_to_cpu_16(e->_ver_vlan)& 0xFFF;
}

static inline uint16_t _erspan_sess_id(struct ERSPAN *e) {
    return rte_be_to_cpu_16(e->_flag1)& 0x3FF ;
}


static inline uint32_t _erspan_src_id(struct ERSPAN *e) {
    return rte_be_to_cpu_32(e->option1)& 0xFFFFF ;
}

static inline uint8_t _erspan_direction(struct ERSPAN *e) {
    return (e->flag2 & 0x8) >> 3 ;
}

static inline uint64_t _erspan_timestamp(struct ERSPAN *e) {
    rte_be64_t timestamp = rte_be_to_cpu_32(e->timestamp_msb);
    timestamp = timestamp << 32;
    timestamp += rte_be_to_cpu_32(e->timestamp);
    return timestamp;
}

static inline struct ERSPAN *erspan_hdr(struct rte_mbuf *pkt)
{
    return (struct ERSPAN *)(rte_pktmbuf_mtod(pkt, char *) + sizeof(struct rte_ether_hdr) + sizeof(struct rte_ipv4_hdr));
}

