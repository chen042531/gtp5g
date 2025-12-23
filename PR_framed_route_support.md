# Support Framed Route for GTP5G

## Summary

This PR adds Framed Route support to the gtp5g kernel module, enabling UPF to handle traffic destined for networks behind UE devices (e.g., CPE scenarios where UE acts as a router for a LAN).

- Add new `framed_route_hash` hash table for efficient downlink PDR lookup by network address
- Implement CIDR-based framed route parsing and matching
- Extend SDF filter matching to consider framed routes for both uplink and downlink
- Use SKB mark to pass prefix length from routing subsystem for downlink lookups

## Background

In traditional 5G deployments, traffic is routed based on UE IP address. However, in **Framed Route** scenarios (3GPP TS 29.244), the UE may act as a router for devices behind it. This means:

- **Downlink**: Packets destined for `10.60.0.0/24` (a network behind UE) should be tunneled to that UE
- **Uplink**: Packets from `10.60.0.0/24` should be recognized as coming from a valid session

## Design Overview

### Packet Matching Changes

#### Downlink (Core → UE)

**Before:**
```
Packet dst IP → addr_hash lookup (exact UE IP match) → PDR
```

**After:**
```
                    ┌─ mark == 0 ─→ addr_hash lookup (UE IP) ─→ PDR
Packet dst IP ──────┤
                    └─ mark != 0 ─→ apply mask ─→ framed_route_hash lookup ─→ PDR
```

The SKB mark carries the prefix length (e.g., `24` for `/24`), which is set by external routing rules (e.g., `ip rule fwmark` or `iptables -j MARK`). This allows the kernel module to:

1. Apply the prefix mask to the destination IP
2. Look up the masked network address in `framed_route_hash`
3. Find the correct PDR for framed route traffic

#### Uplink (UE → Core)

**Before:**
```
GTP packet → F-TEID hash lookup → PDR → SDF filter (exact IP match)
```

**After:**
```
GTP packet → F-TEID hash lookup → PDR → SDF filter (IP match OR framed route match)
```

For uplink packets:
- First match by F-TEID (GTP tunnel ID) - unchanged
- SDF filter now checks if source IP matches UE IP **OR** any configured framed route
- This allows packets from networks behind the UE to pass SDF filtering

### New Data Structures

```c
// New hash node for framed route lookup
struct framed_route_node {
    struct hlist_node hlist;
    struct pdr *pdr;
    __be32 network_addr;    // Network address (after masking)
    __be32 netmask;         // Netmask for the framed route
};

// Added to PDI structure
struct pdi {
    ...
    u32 framed_route_num;
    struct framed_route_node **framed_route_nodes;
};

// New hash table in gtp5g_dev
struct gtp5g_dev {
    ...
    struct hlist_head *framed_route_hash;  // For downlink framed route lookup
};
```

### Key Functions Added

| Function | Purpose |
|----------|---------|
| `pdr_find_by_framed_route()` | Hash-based PDR lookup by masked network address |
| `parse_framed_route_cidr()` | Parse CIDR string (e.g., "10.60.0.0/24") to network/mask |
| `ip_match_with_framed_routes()` | Check if IP matches rule or any framed route |
| `get_skb_routing_mark()` | Get SKB mark containing prefix length |
| `apply_prefix_mask_to_ipv4()` | Apply prefix mask to IPv4 address |

### Netlink Interface

New PDI attribute `GTP5G_PDI_FRAMED_ROUTE` accepts nested CIDR strings:

```
GTP5G_PDI_FRAMED_ROUTE
  ├── "10.60.0.0/24"
  ├── "192.168.100.0/24"
  └── ...
```

## Optimization: Only Downlink PDRs in framed_route_hash

Only **downlink** PDRs are added to `framed_route_hash` because:

1. Uplink PDRs already use `i_teid_hash` for lookup (match by GTP tunnel ID)
2. Framed route hash is only needed for downlink where we match by destination network
3. This reduces hash table size and collision probability

## Files Changed

| File | Changes |
|------|---------|
| `include/dev.h` | Add `framed_route_hash` to device structure |
| `include/pdr.h` | Add `framed_route_node` struct and function declarations |
| `include/genl_pdr.h` | Add `GTP5G_PDI_FRAMED_ROUTE` attribute |
| `include/util.h` | Add `get_skb_routing_mark()` declaration |
| `src/genl/genl_pdr.c` | Parse and fill framed routes via netlink |
| `src/gtpu/dev.c` | Initialize and free `framed_route_hash` |
| `src/gtpu/encap.c` | Use SKB mark for framed route downlink lookup |
| `src/pfcp/pdr.c` | Implement framed route matching and hash management |
| `src/util.c` | Implement `get_skb_routing_mark()` |

## Usage Example

### SMF/UPF Configuration

Configure PDR with framed routes:
```
PDR:
  PDI:
    Source Interface: Core
    UE IP: 10.60.0.1
    Framed Routes:
      - 10.60.0.0/24
      - 192.168.100.0/24
```

### Linux Routing Setup

Set up routing rules to mark packets with prefix length:
```bash
# Mark packets destined for framed route networks
iptables -t mangle -A PREROUTING -d 10.60.0.0/24 -j MARK --set-mark 24
iptables -t mangle -A PREROUTING -d 192.168.100.0/24 -j MARK --set-mark 24

# Or use ip rule with fwmark
ip rule add to 10.60.0.0/24 fwmark 24 table upf
```

## Test Plan

- [ ] Verify downlink packets to framed route networks are correctly tunneled to UE
- [ ] Verify uplink packets from framed route networks pass SDF filtering
- [ ] Verify PDR with multiple framed routes works correctly
- [ ] Verify PDR precedence is respected in framed_route_hash
- [ ] Verify cleanup on PDR deletion (no memory leaks)
- [ ] Verify backward compatibility (PDRs without framed routes work as before)

---

🤖 Generated with [Claude Code](https://claude.com/claude-code)


  摘要

  這次修改的核心設計：

  Downlink (封包從 Core → UE)

  - 新增 framed_route_hash 哈希表，用 network address 作為 key
  - 利用 SKB mark 傳遞 prefix length（由外部 iptables/ip rule 設定）
  - 封包進來時：若 mark != 0，套用 mask 後查 framed_route_hash；否則走原本的 addr_hash

  Uplink (封包從 UE → Core)

  - 仍用 F-TEID 哈希表查 PDR（不變）
  - SDF filter 擴展：source IP 可以匹配 UE IP 或 任何 framed route 網段
  - 這樣從 UE 後面網路來的封包也能通過 SDF 過濾

  為什麼需要這樣設計

  傳統 PDR 只匹配單一 UE IP，但 Framed Route 場景中 UE 充當 router，流量目的地是 UE 背後的網段（如 10.60.0.0/24）。這個設計讓 UPF 能：
  1. 正確識別 downlink 流量並封裝到