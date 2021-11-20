# zSpan

zSpan is a high performance packet collector for Cisco ERSPAN based datacenter switch.
Cisco Nexus 9000 series datacenter switch could use ERSPAN Type-3 header provide high percision(100ps) timestamp.

## ERSPAN Header format on Nexus 9000

```
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
```


### Config ERSPAN on Cisco Switch

```bash
SW-100G(config)# feature interface-vlan 
SW-100G(config)# vlan 123
SW-100G(config-vlan)# name ERSPAN
SW-100G(config-vlan)# interface vlan 123

SW-100G(config)# interface vlan 123
SW-100G(config-if)# ip address 11.11.11.1 255.255.255.0
SW-100G(config-if)# no shut
SW-100G(config-if)# interface Eth1/29
SW-100G(config-if)# switchport 
SW-100G(config-if)# switchport mode access 
SW-100G(config-if)# switchport access vlan 123
SW-100G(config-if)# no shut

SW-100G(config)# monitor erspan origin ip-address 11.11.11.1 global 

SW-100G(config)# monitor session 3 type erspan-source
SW-100G(config-erspan-src)#   vrf default 
SW-100G(config-erspan-src)#   header-type 3
SW-100G(config-erspan-src)#   erspan-id 156
SW-100G(config-erspan-src)#   destination ip 11.11.11.11
SW-100G(config-erspan-src)#   ip ttl 64
SW-100G(config-erspan-src)#   source interface Ethernet1/31 both
SW-100G(config-erspan-src)#   mtu 1518
SW-100G(config-erspan-src)#   no shut

SW-100G# show monitor session 3
   session 3
---------------
type              : erspan-source
version           : 3
state             : up
erspan-id         : 156
vrf-name          : default
acl-name          : acl-name not specified
ip-ttl            : 64
ip-dscp           : 0
header-type       : 3
mtu               : 1518
destination-ip    : 11.11.11.11
origin-ip         : 11.11.11.1 (global)
source intf       : 
    rx            : Eth1/31       
    tx            : Eth1/31       
    both          : Eth1/31       
source VLANs      : 
    rx            : 
    tx            : 
    both          : 
filter VLANs      : filter not specified
source fwd drops  : 

marker-packet     : disabled
packet interval   : 100
packet sent       : 0
packet failed     : 0
egress-intf       : Eth1/29
source VSANs      : 
    rx            : 
```

### Start zSPAN Collector

When system bring up, it will automatically create a virtual interface to handle IP connection.
Only GRE packets will redirected to `pkt_processing` function.

This is a simple framework and shows some basic examples to fetch timestamp and port information from erspan packet.
You may add your own code logic in  `pkt_processing`.


```bash
s[root@c220m4-Gen2 zSpan]# ./build/zspan -a 0000:0e:00.1 -- --addr 11.11.11.11 --netmask 255.255.255.0 --core_num 16


seq:    108 | vlan:     0 | sess-id:  156 | src_id:     48 | dir:  1 | timestamp: 143991653282609 |1.0.0.2 -> 1.0.0.1
seq:    104 | vlan:     0 | sess-id:  156 | src_id:    276 | dir:  0 | timestamp: 143991653289409 |1.0.0.1 -> 1.0.0.2
seq:    109 | vlan:     0 | sess-id:  156 | src_id:     48 | dir:  1 | timestamp: 143992660663409 |1.0.0.2 -> 1.0.0.1
seq:    105 | vlan:     0 | sess-id:  156 | src_id:    276 | dir:  0 | timestamp: 143992660671169 |1.0.0.1 -> 1.0.0.2

```
