#include <linux/version.h>
#include <linux/types.h>

#include "dev.h"
#include "link.h"
#include "pdr.h"
#include "far.h"
#include "gtp.h"
#include "genl_pdr.h"
#include "genl_far.h"
#include "seid.h"
#include "hash.h"
#include "genl.h"
#include "log.h"
#include "util.h"


int qos_enable = 0; // set QoS disable as default value

int get_qos_enable()
{
    return qos_enable;
}

void set_qos_enable(int val)
{
    qos_enable = val;
}

static void seid_pdr_id_to_hex_str(u64 seid_int, u16 pdr_id, char *buff)
{
    seid_and_u32id_to_hex_str(seid_int, (u32)(pdr_id), buff);
}

static void pdr_context_free(struct rcu_head *head)
{
    struct pdr *pdr = container_of(head, struct pdr, rcu_head);
    struct pdi *pdi;
    struct sdf_filter *sdf;

    if (!pdr)
        return;

    sock_put(pdr->sk);

    if (pdr->outer_header_removal)
        kfree(pdr->outer_header_removal);

    pdi = pdr->pdi;
    if (pdi) {
        if (pdi->ue_addr_ipv4)
            kfree(pdi->ue_addr_ipv4);
        if (pdi->f_teid)
            kfree(pdi->f_teid);
        if (pdr->far_id)
            kfree(pdr->far_id);
        if (pdr->qer_ids)
            kfree(pdr->qer_ids);
        if (pdr->urr_ids)
            kfree(pdr->urr_ids);
        
        // Clean up framed route nodes
        if (pdi->framed_route_nodes) {
            int j;
            for (j = 0; j < pdi->framed_route_num; j++) {
                if (pdi->framed_route_nodes[j])
                    kfree(pdi->framed_route_nodes[j]);
            }
            kfree(pdi->framed_route_nodes);
        }
        
        sdf = pdi->sdf;
        if (sdf) {
            if (sdf->rule) {
                if (sdf->rule->sport)
                    kfree(sdf->rule->sport);
                if (sdf->rule->dport)
                    kfree(sdf->rule->dport);
                kfree(sdf->rule);
            }
            if (sdf->tos_traffic_class)
                kfree(sdf->tos_traffic_class);
            if (sdf->security_param_idx)
                kfree(sdf->security_param_idx);
            if (sdf->flow_label)
                kfree(sdf->flow_label);
            if (sdf->bi_id)
                kfree(sdf->bi_id);

            kfree(sdf);
        }
        kfree(pdi);
    }

    unix_sock_client_delete(pdr);
    kfree(pdr);
}

void pdr_context_delete(struct pdr *pdr)
{
    struct pdi *pdi;
    int j;

    if (!pdr)
        return;

    if (!hlist_unhashed(&pdr->hlist_id))
        hlist_del_rcu(&pdr->hlist_id);

    if (!hlist_unhashed(&pdr->hlist_i_teid))
        hlist_del_rcu(&pdr->hlist_i_teid);

    if (!hlist_unhashed(&pdr->hlist_addr))
        hlist_del_rcu(&pdr->hlist_addr);

    // Delete all framed route nodes from hash table
    pdi = pdr->pdi;
    if (pdi && pdi->framed_route_nodes) {
        for (j = 0; j < pdi->framed_route_num; j++) {
            if (pdi->framed_route_nodes[j] && 
                !hlist_unhashed(&pdi->framed_route_nodes[j]->hlist)) {
                hlist_del_rcu(&pdi->framed_route_nodes[j]->hlist);
            }
        }
    }

    call_rcu(&pdr->rcu_head, pdr_context_free);
}

// Delete the AF_UNIX client
void unix_sock_client_delete(struct pdr *pdr)
{
    if (!pdr || pdr_addr_is_netlink(pdr))
        return;

    if (pdr->sock_for_buf)
        sock_release(pdr->sock_for_buf);

    pdr->sock_for_buf = NULL;
}

// Create a AF_UNIX client by specific name sent from user space
int unix_sock_client_new(struct pdr *pdr)
{
    struct socket **psock = &pdr->sock_for_buf;
    struct sockaddr_un *addr = &pdr->addr_unix;
    int err;

    if (strlen(addr->sun_path) == 0) {
        return -EINVAL;
    }

    if (pdr_addr_is_netlink(pdr)) {
        return 0;
    }

    err = sock_create(AF_UNIX, SOCK_DGRAM, 0, psock);
    if (err) {
        return err;
    }

    err = (*psock)->ops->connect(*psock, (struct sockaddr *)addr,
            sizeof(addr->sun_family) + strlen(addr->sun_path), 0);
    if (err) {
        unix_sock_client_delete(pdr);
        return err;
    }

    return 0;
}

// Handle PDR/FAR changed and affect buffering
int unix_sock_client_update(struct pdr *pdr, struct far *far)
{
    if (!pdr || pdr_addr_is_netlink(pdr))
        return 0;

    unix_sock_client_delete(pdr);

    if ((far && (far->action & FAR_ACTION_BUFF)) || pdr->urr_num > 0)
        return unix_sock_client_new(pdr);

    return 0;
}

struct pdr *find_pdr_by_id(struct gtp5g_dev *gtp, u64 seid, u16 pdr_id)
{
    struct hlist_head *head;
    struct pdr *pdr;
    char seid_pdr_id[SEID_U32ID_HEX_STR_LEN] = {0};

    seid_pdr_id_to_hex_str(seid, pdr_id, seid_pdr_id);
    head = &gtp->pdr_id_hash[str_hashfn(seid_pdr_id) % gtp->hash_size];
    hlist_for_each_entry_rcu(pdr, head, hlist_id) {
        if (pdr->seid == seid && pdr->id == pdr_id)
            return pdr;
    }

    return NULL;
}

static int ipv4_match(__be32 target_addr, __be32 ifa_addr, __be32 ifa_mask)
{
    return !((target_addr ^ ifa_addr) & ifa_mask);
}

static bool ports_match(struct range *match_list, int list_len, __be16 port)
{
    int i;

    if (!list_len)
        return true;

    for (i = 0; i < list_len; i++) {
        if (match_list[i].start <= port && match_list[i].end >= port)
            return true;
    }
    return false;
}

// Helper function to check if IP matches rule or framed routes
static bool ip_match_with_framed_routes(__be32 ip_addr, __be32 rule_addr, __be32 rule_mask, struct pdi *pdi)
{
    struct framed_route_node *node;
    int j;

    PRINTK_TIME("ip_match_with_framed_routes: ip_addr: %pI4, rule_addr: %pI4, rule_mask: %pI4\n", &ip_addr, &rule_addr, &rule_mask);
    // First try exact match with rule
    if (ipv4_match(ip_addr, rule_addr, rule_mask)) {
        PRINTK_TIME("IP %pI4 matches rule %pI4/%pI4\n", &ip_addr, &rule_addr, &rule_mask);
        return true;
    }

    // If no exact match, check framed routes
    if (!pdi || !pdi->framed_route_nodes || pdi->framed_route_num == 0) {
        PRINTK_TIME("IP %pI4 no framed routes to check\n", &ip_addr);
        return false;
    }

    for (j = 0; j < pdi->framed_route_num; j++) {
        node = pdi->framed_route_nodes[j];
        if (node && ipv4_match(ip_addr, node->network_addr, node->netmask)) {
            PRINTK_TIME("IP %pI4 matches framed route %pI4/%pI4\n",
                      &ip_addr, &node->network_addr, &node->netmask);
            return true;
        }
    }

    PRINTK_TIME("IP %pI4 no match in any framed routes (%d routes checked)\n", &ip_addr, pdi->framed_route_num);
    return false;
}

static int sdf_filter_match(struct pdr *pdr, struct sk_buff *skb, 
    unsigned int hdrlen, u8 direction)
{
    #define IP_PROTO_RESERVED 0xff
    struct iphdr *iph;
    struct ip_filter_rule *rule;
    struct sdf_filter *sdf;
    struct pdi *pdi;
    const __be16 *pptr;
    __be16 _ports[2];

    if (!pdr || !pdr->pdi || !pdr->pdi->sdf)
        return 1;

    PRINTK_TIME("===== sdf_filter_match: PDR ID: %u pktsrc: %pI4, pktdst: %pI4 =====\n", pdr->id, &iph->saddr, &iph->daddr);
    pdi = pdr->pdi;
    sdf = pdi->sdf;

    if (!pskb_may_pull(skb, hdrlen + sizeof(struct iphdr)))
        goto mismatch;

    iph = (struct iphdr *)(skb->data + hdrlen);

    if (sdf->rule) {
        rule = sdf->rule;
        if (rule->direction != direction)
            goto mismatch;

        if (rule->proto != IP_PROTO_RESERVED && rule->proto != iph->protocol)
            goto mismatch;

        // Check source and destination IP (with framed route support)
        if (is_uplink(pdr)) { // Uplink
            PRINTK_TIME("===== sdf_filter_match: Uplink src: %pI4, dst: %pI4 =====\n", &iph->saddr, &iph->daddr);
            // Source: match rule OR framed routes
            if (!ip_match_with_framed_routes(iph->saddr, rule->src.s_addr, rule->smask.s_addr, pdi))
                goto mismatch;

            // Destination: exactly match rule
            if (!ipv4_match(iph->daddr, rule->dest.s_addr, rule->dmask.s_addr))
                goto mismatch;
        } else { // Downlink
            PRINTK_TIME("===== sdf_filter_match: Downlink =====\n");
            // Source: exactly match rule
            if (!ipv4_match(iph->saddr, rule->src.s_addr, rule->smask.s_addr))
                goto mismatch;

            // Destination: match rule OR framed routes
            if (!ip_match_with_framed_routes(iph->daddr, rule->dest.s_addr, rule->dmask.s_addr, pdi))
                goto mismatch;
        } 

        if (rule->sport_num + rule->dport_num > 0) {
            if (!(pptr = skb_header_pointer(skb, hdrlen + sizeof(struct iphdr), sizeof(_ports), _ports)))
                goto mismatch;

            if (!ports_match(rule->sport, rule->sport_num, ntohs(pptr[0])))
                goto mismatch;

            if (!ports_match(rule->dport, rule->dport_num, ntohs(pptr[1])))
                goto mismatch;
        }
    }

/*
    if (sdf->tos_traffic_class)
        GTP5G_ERR(NULL, "TODO: SDF's ToS traffic class\n");

    if (sdf->security_param_idx)
        GTP5G_ERR(NULL, "TODO: SDF's Security parameter index\n");

    if (sdf->flow_label)
        GTP5G_ERR(NULL, "TODO: SDF's Flow label\n");

    if (sdf->bi_id)
        GTP5G_ERR(NULL, "TODO: SDF's SDF filter id\n");
*/

    return 1;
mismatch:
    return 0;
}

// This function checks whether an IP matches.
// It uses the PDR direction to decide matching UE address
// with the src address or dst address of receving packet.
// If it is matched, it returns True; otherwise, it returns False.
bool ip_match(struct iphdr *iph, struct pdr *pdr)
{
    struct pdi *pdi = pdr->pdi;
    struct framed_route_node *node;
    __be32 target_addr;
    int j;

    if (!pdi)
        return false;

    // Determine target address based on PDR direction (uplink or downlink)
    target_addr = is_uplink(pdr) ? iph->saddr : iph->daddr;

    // First, try to match by ue_addr_ipv4
    if (pdi->ue_addr_ipv4 && pdi->ue_addr_ipv4->s_addr == target_addr)
        return true;

    // If ue_addr_ipv4 doesn't match, check if target_addr is in framed route range
    if (!pdi->framed_route_nodes || pdi->framed_route_num == 0)
        return false;

    // Check each framed route
    for (j = 0; j < pdi->framed_route_num; j++) {
        node = pdi->framed_route_nodes[j];
        if (!node)
            continue;

        // Check if target IP is in this network range
        if (ipv4_match(target_addr, node->network_addr, node->netmask))
            return true;
    }

    return false;
}

struct pdr *pdr_find_by_gtp1u(struct gtp5g_dev *gtp, struct sk_buff *skb,
        unsigned int hdrlen, u32 teid, u8 type)
{
#ifdef MATCH_IP
    struct iphdr *outer_iph;
#endif
    struct iphdr *iph;
    struct hlist_head *head;
    struct pdr *pdr;
    struct pdi *pdi;

    if (!gtp) {
        return NULL;
    }

    if (ntohs(skb->protocol) != ETH_P_IP) {
        return NULL;
    }

    if (type == GTPV1_MSG_TYPE_TPDU) {
        if (!pskb_may_pull(skb, hdrlen + sizeof(struct iphdr))) {
            return NULL;
        }
    }

    head = &gtp->i_teid_hash[u32_hashfn(teid) % gtp->hash_size];
    hlist_for_each_entry_rcu(pdr, head, hlist_i_teid) {
        pdi = pdr->pdi;
        if (!pdi) {
            continue;
        }

        // GTP-U packet must check teid
        if (!(pdi->f_teid && pdi->f_teid->teid == teid)) {
            continue;
        }

        if (type != GTPV1_MSG_TYPE_TPDU) {
            return pdr;
        }

        // check outer IP dest addr to distinguish between N3 and N9 packet while act as i-upf
#ifdef MATCH_IP
    #if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 0, 0)
        outer_iph = (struct iphdr *)(skb->head + skb->network_header);
        if (!(pdi->f_teid && pdi->f_teid->gtpu_addr_ipv4.s_addr == outer_iph->daddr)) {
            continue;
        }
    #else
        outer_iph = (struct iphdr *)(skb->network_header);
        if (!(pdi->f_teid && pdi->f_teid->gtpu_addr_ipv4.s_addr == outer_iph->daddr)) {
            continue;
        }
    #endif
#endif
        // Check UE IP address or framed routes
        iph = (struct iphdr *)(skb->data + hdrlen); // inner IP header
        if (pdr->af == AF_INET) {
            // ip_match now handles both ue_addr_ipv4 and framed routes
            if (!ip_match(iph, pdr)) {
                continue;
            }
        }

        if (!sdf_filter_match(pdr, skb, hdrlen, GTP5G_SDF_FILTER_OUT)) {
            continue;
        }

        PRINTK_TIME("===== pdr_find_by_gtp1u: FOUND PDR ID: %u =====\n", pdr->id);
        GTP5G_INF(NULL, "Match PDR ID:%d\n", pdr->id);

        return pdr;
    }

    return NULL;
}

struct pdr *pdr_find_by_ipv4(struct gtp5g_dev *gtp, struct sk_buff *skb,
        unsigned int hdrlen, __be32 addr)
{
    struct hlist_head *head;
    struct pdr *pdr;
    struct pdi *pdi;

    PRINTK_TIME("===== pdr_find_by_ipv4: Looking for addr %pI4 =====\n", &addr);

    head = &gtp->addr_hash[ipv4_hashfn(addr) % gtp->hash_size];

    hlist_for_each_entry_rcu(pdr, head, hlist_addr) {
        pdi = pdr->pdi;

        PRINTK_TIME("Checking PDR ID: %u, UE Addr: %pI4\n", pdr->id, &pdi->ue_addr_ipv4->s_addr);

        // TODO: Move the value we check into first level
        if (!(pdr->af == AF_INET && pdi->ue_addr_ipv4->s_addr == addr)) {
            PRINTK_TIME("  PDR ID %u: AF or UE addr mismatch\n", pdr->id);
            continue;
        }

        if (!sdf_filter_match(pdr, skb, hdrlen, GTP5G_SDF_FILTER_OUT)) {
            PRINTK_TIME("  PDR ID %u: SDF filter mismatch\n", pdr->id);
            continue;
        }

        PRINTK_TIME("===== pdr_find_by_ipv4: FOUND PDR ID: %u =====\n", pdr->id);
        return pdr;
    }

    PRINTK_TIME("===== pdr_find_by_ipv4: NO PDR found for addr %pI4 =====\n", &addr);
    return NULL;
}

// Parse CIDR format string (e.g., "192.168.1.0/24") to get network address and mask
int parse_framed_route_cidr(const char *route_str, __be32 *network_addr,
        __be32 *netmask)
{
    char route_copy[64];
    char *slash_pos;
    int prefix_len;
    u32 addr[4];
    u32 mask;
    int ret;

    if (!route_str || !network_addr || !netmask)
        return -EINVAL;

    // Copy string to avoid modifying original
    strncpy(route_copy, route_str, sizeof(route_copy) - 1);
    route_copy[sizeof(route_copy) - 1] = '\0';

    // Find the slash
    slash_pos = strchr(route_copy, '/');
    if (!slash_pos)
        return -EINVAL;

    // Replace '/' with '\0' to split the string into two parts: IP address and prefix length
    *slash_pos = '\0';
    slash_pos++;  // Move pointer to the start of prefix length string

    // Parse IP address
    ret = sscanf(route_copy, "%u.%u.%u.%u", &addr[0], &addr[1], &addr[2], &addr[3]);
    if (ret != 4)
        return -EINVAL;

    // Parse prefix length
    ret = kstrtoint(slash_pos, 10, &prefix_len);
    if (ret < 0 || prefix_len < 0 || prefix_len > 32)
        return -EINVAL;

    // Calculate network address
    *network_addr = htonl((addr[0] << 24) | (addr[1] << 16) | (addr[2] << 8) | addr[3]);

    // Calculate netmask
    if (prefix_len == 0)
        mask = 0;
    else
        mask = 0xFFFFFFFF << (32 - prefix_len);
    *netmask = htonl(mask);

    // Apply mask to get network address
    *network_addr = *network_addr & *netmask;

    return 0;
}

struct pdr *pdr_find_by_framed_route(struct gtp5g_dev *gtp, struct sk_buff *skb,
        unsigned int hdrlen, __be32 masked_addr)
{
    struct hlist_head *head;
    struct pdr *pdr;
    struct framed_route_node *node;

    if (!gtp)
        return NULL;

    PRINTK_TIME("===== pdr_find_by_framed_route: Looking for masked_addr %pI4 =====\n", &masked_addr);

    head = &gtp->framed_route_hash[ipv4_hashfn(masked_addr) % gtp->hash_size];

    // Search framed_route_nodes by masked network address
    hlist_for_each_entry_rcu(node, head, hlist) {
        if (!node->pdr) {
            PRINTK_TIME("Framed route node has no PDR\n");
            continue;
        }

        pdr = node->pdr;

        if (is_uplink(node->pdr)) 
            continue;
        // Match by network address
        if (node->network_addr != masked_addr) {
            PRINTK_TIME("Network addr %pI4 != masked_addr %pI4\n", &node->network_addr, &masked_addr);
            continue;
        }

        if (!sdf_filter_match(pdr, skb, hdrlen, GTP5G_SDF_FILTER_OUT)) {
            PRINTK_TIME("PDR ID %u: SDF filter mismatch\n", pdr->id);
            continue;
        }

        PRINTK_TIME("===== pdr_find_by_framed_route: FOUND PDR ID: %u =====\n", pdr->id);
        return pdr;
    }

    PRINTK_TIME("===== pdr_find_by_framed_route: NO PDR found for masked_addr %pI4 =====\n", &masked_addr);
    return NULL;
}

void pdr_append(u64 seid, u16 pdr_id, struct pdr *pdr, struct gtp5g_dev *gtp)
{
    u32 i;
    char seid_pdr_id_hexstr[SEID_U32ID_HEX_STR_LEN] = {0};

    seid_pdr_id_to_hex_str(seid, pdr_id, seid_pdr_id_hexstr);
    i = str_hashfn(seid_pdr_id_hexstr) % gtp->hash_size;
    hlist_add_head_rcu(&pdr->hlist_id, &gtp->pdr_id_hash[i]);
}

void pdr_update_hlist_table(struct pdr *pdr, struct gtp5g_dev *gtp)
{
    struct hlist_head *head;
    struct pdr *ppdr;
    struct pdr *last_ppdr;
    struct pdi *pdi;
    struct local_f_teid *f_teid;
    struct framed_route_node *node;
    struct framed_route_node *last_node;
    int j;

    if (!hlist_unhashed(&pdr->hlist_i_teid))
        hlist_del_rcu(&pdr->hlist_i_teid);

    if (!hlist_unhashed(&pdr->hlist_addr))
        hlist_del_rcu(&pdr->hlist_addr);

    pdi = pdr->pdi;
    if (!pdi)
        return;

    // Delete old framed route nodes
    if (pdi->framed_route_nodes) {
        for (j = 0; j < pdi->framed_route_num; j++) {
            if (pdi->framed_route_nodes[j] && 
                !hlist_unhashed(&pdi->framed_route_nodes[j]->hlist)) {
                hlist_del_rcu(&pdi->framed_route_nodes[j]->hlist);
            }
        }
    }

    f_teid = pdi->f_teid;
    if (f_teid) {
        last_ppdr = NULL;
        head = &gtp->i_teid_hash[u32_hashfn(f_teid->teid) % gtp->hash_size];
        hlist_for_each_entry_rcu(ppdr, head, hlist_i_teid) {
            if (pdr->precedence > ppdr->precedence)
                last_ppdr = ppdr;
            else
                break;
        }
        if (!last_ppdr)
            hlist_add_head_rcu(&pdr->hlist_i_teid, head);
        else
            hlist_add_behind_rcu(&pdr->hlist_i_teid, &last_ppdr->hlist_i_teid);
    } else if (pdi->ue_addr_ipv4) {
        last_ppdr = NULL;
        head = &gtp->addr_hash[u32_hashfn(pdi->ue_addr_ipv4->s_addr) % gtp->hash_size];
        hlist_for_each_entry_rcu(ppdr, head, hlist_addr) {
            if (pdr->precedence > ppdr->precedence)
                last_ppdr = ppdr;
            else
                break;
        }
        if (!last_ppdr)
            hlist_add_head_rcu(&pdr->hlist_addr, head);
        else
            hlist_add_behind_rcu(&pdr->hlist_addr, &last_ppdr->hlist_addr);
    }
    
    // Process framed routes independently (can coexist with f_teid or ue_addr_ipv4)
    if (pdi->framed_route_nodes && pdi->framed_route_num > 0) {
        for (j = 0; j < pdi->framed_route_num; j++) {
            struct framed_route_node *fr_node = pdi->framed_route_nodes[j];

            if (!fr_node) {
                continue;
            }

            fr_node->pdr = pdr;

            if (!hlist_unhashed(&fr_node->hlist))
                hlist_del_rcu(&fr_node->hlist);

            head = &gtp->framed_route_hash[ipv4_hashfn(fr_node->network_addr) % gtp->hash_size];
            last_node = NULL;

            // Find the correct position based on precedence
            hlist_for_each_entry_rcu(node, head, hlist) {
                if (!node->pdr)
                    continue;
                if (pdr->precedence > node->pdr->precedence)
                    last_node = node;
                else
                    break;
            }

            if (!last_node)
                hlist_add_head_rcu(&fr_node->hlist, head);
            else
                hlist_add_behind_rcu(&fr_node->hlist, &last_node->hlist);
        }
    }
}

bool is_uplink(struct pdr *pdr)
{
    if (!pdr || !pdr->pdi)
        return false;
    return (pdr->pdi->srcIntf == SRC_INTF_ACCESS);
}

bool is_downlink(struct pdr *pdr)
{
    if (!pdr || !pdr->pdi)
        return false;
    return (pdr->pdi->srcIntf == SRC_INTF_CORE);
}
