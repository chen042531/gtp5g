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

// Check if IP address matches any framed route in PDI
static bool ip_match_framed_routes(struct iphdr *iph, struct pdr *pdr)
{
    struct pdi *pdi = pdr->pdi;
    __be32 target_addr;
    struct framed_route_node *node;
    int j;
    
    if (!pdi || !pdi->framed_route_nodes || pdi->framed_route_num == 0)
        return false;
    
    // For uplink, check source IP; for downlink, check dest IP
    if (is_uplink(pdr)) {
        target_addr = iph->saddr;
    } else {
        target_addr = iph->daddr;
    }
    
    printk("GTP5G: %s - target_addr=%pI4\n", __func__, &target_addr);
    printk("GTP5G: %s - pdi->framed_route_num=%d\n", __func__, pdi->framed_route_num);
    // Check each framed route
    for (j = 0; j < pdi->framed_route_num; j++) {
        node = pdi->framed_route_nodes[j];
        if (!node)
            continue;
            
        printk("GTP5G: %s - pdi->framed_route_nodes[%d]=%p\n", __func__, j, node);
        printk("GTP5G: %s - network_addr=%pI4, netmask=%pI4\n", __func__,
               &node->network_addr, &node->netmask);
        // Check if target IP is in this network range
        {
            __be32 xor_result = target_addr ^ node->network_addr;
            __be32 masked_xor = xor_result & node->netmask;
            printk("GTP5G: %s - target_addr=%pI4, network_addr=%pI4, netmask=%pI4\n",
                   __func__, &target_addr, &node->network_addr, &node->netmask);
            printk("GTP5G: %s - XOR result=%pI4, masked XOR=%pI4, match=%d\n",
                   __func__, &xor_result, &masked_xor, (masked_xor == 0));
        }
        if (ipv4_match(target_addr, node->network_addr, node->netmask)) {
            char route_repr[40];
            u8 prefix = netmask_to_prefix(node->netmask);

            printk("GTP5G: %s - ipv4_match returned TRUE! Calculating prefix...\n", __func__);
            printk("GTP5G: %s - netmask=%pI4, prefix=%u\n", __func__, &node->netmask, prefix);
            
            snprintf(route_repr, sizeof(route_repr), "%pI4/%u",
                     &node->network_addr, prefix);
            
            printk("GTP5G: %s - route_repr=%s, is_uplink=%d\n", __func__, route_repr, is_uplink(pdr));
            
            if (is_uplink(pdr)) {
                printk("GTP5G: Uplink: Framed route matched! PDR ID=%u, Source IP=%pI4, Framed Route=%s\n", 
                       pdr->id, &target_addr, route_repr);
                GTP5G_INF(NULL, "Uplink: Framed route matched! PDR ID=%u, Source IP=%pI4, Framed Route=%s\n", 
                          pdr->id, &target_addr, route_repr);
            } else {
                printk("GTP5G: Downlink: Framed route matched! PDR ID=%u, Dest IP=%pI4, Framed Route=%s\n", 
                       pdr->id, &target_addr, route_repr);
                GTP5G_INF(NULL, "Downlink: Framed route matched! PDR ID=%u, Dest IP=%pI4, Framed Route=%s\n", 
                          pdr->id, &target_addr, route_repr);
            }
            printk("GTP5G: %s - Returning TRUE\n", __func__);
            return true;
        } else {
            printk("GTP5G: %s - ipv4_match returned FALSE for node[%d]\n", __func__, j);
        }
    }
    
    return false;
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

static int sdf_filter_match(struct sdf_filter *sdf, struct sk_buff *skb,
        unsigned int hdrlen, u8 direction)
{
    #define IP_PROTO_RESERVED 0xff
    struct iphdr *iph;
    struct ip_filter_rule *rule;
    const __be16 *pptr;
    __be16 _ports[2];

    if (!sdf)
        return 1;

    if (!pskb_may_pull(skb, hdrlen + sizeof(struct iphdr)))
        goto mismatch;

    iph = (struct iphdr *)(skb->data + hdrlen);

    if (sdf->rule) {
        rule = sdf->rule;
        if (rule->direction != direction)
            goto mismatch;

        if (rule->proto != IP_PROTO_RESERVED && rule->proto != iph->protocol)
            goto mismatch;

        if (!ipv4_match(iph->saddr, rule->src.s_addr, rule->smask.s_addr))
            goto mismatch;

        if (!ipv4_match(iph->daddr, rule->dest.s_addr, rule->dmask.s_addr))
            goto mismatch;

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
    if (!pdi)
        return false;

    if (is_uplink(pdr)) {
        return pdi->ue_addr_ipv4->s_addr == iph->saddr;
    } else {
        return pdi->ue_addr_ipv4->s_addr == iph->daddr;
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
            bool ip_matched = false;
            
            // First try to match by ue_addr_ipv4
            if (pdi->ue_addr_ipv4) {
                printk("GTP5G: %s - ue_addr_ipv4=%pI4, iph->saddr=%pI4\n", __func__, pdi->ue_addr_ipv4, &iph->saddr);
                if (ip_match(iph, pdr)) {
                    ip_matched = true;
                }
            }
            
            // If ue_addr_ipv4 doesn't match, try framed routes
            if (!ip_matched) {
                printk("GTP5G: %s - ip_match_framed_routes\n", __func__);
                if (ip_match_framed_routes(iph, pdr)) {
                    ip_matched = true;
                }
            }
            
            // If neither matched, continue to next PDR
            if (!ip_matched) {
                continue;
            }
        }

        if (pdi->sdf) {
            printk("GTP5G: %s - Checking SDF filter for PDR ID=%u\n", __func__, pdr->id);
            if (!sdf_filter_match(pdi->sdf, skb, hdrlen, GTP5G_SDF_FILTER_OUT)) {
                printk("GTP5G: %s - SDF filter match failed for PDR ID=%u, continue\n", __func__, pdr->id);
                continue;
            }
            printk("GTP5G: %s - SDF filter match passed for PDR ID=%u\n", __func__, pdr->id);
        }

        printk("GTP5G: %s - Match PDR ID:%d\n", __func__, pdr->id);
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

    head = &gtp->addr_hash[ipv4_hashfn(addr) % gtp->hash_size];

    hlist_for_each_entry_rcu(pdr, head, hlist_addr) {
        pdi = pdr->pdi;

        // TODO: Move the value we check into first level
        if (!(pdr->af == AF_INET && pdi->ue_addr_ipv4->s_addr == addr))
            continue;

        if (pdi->sdf)
            if (!sdf_filter_match(pdi->sdf, skb, hdrlen, GTP5G_SDF_FILTER_OUT))
                continue;

        return pdr;
    }

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
    struct pdi *pdi;
    struct framed_route_node *node;

    printk("GTP5G: %s - start finding PDR by framed route\n", __func__);
    if (!gtp)
        return NULL;

    printk("GTP5G: %s - gtp is not NULL\n", __func__);

    head = &gtp->framed_route_hash[ipv4_hashfn(masked_addr) % gtp->hash_size];

    printk("GTP5G: %s - head is not NULL, searching for masked_addr=%pI4\n", __func__, &masked_addr);

    // Search framed_route_nodes by masked network address
    hlist_for_each_entry_rcu(node, head, hlist) {
        printk("GTP5G: %s - found node, checking...\n", __func__);
        
        if (!node->pdr) {
            printk("GTP5G: %s - node->pdr is NULL, skip\n", __func__);
            continue;
        }

        printk("GTP5G: %s - node->network_addr=%pI4, looking for=%pI4\n", 
               __func__, &node->network_addr, &masked_addr);

        // Match by network address
        if (node->network_addr != masked_addr) {
            printk("GTP5G: %s - network_addr mismatch, continue\n", __func__);
            continue;
        }

        printk("GTP5G: %s - network_addr matched!\n", __func__);

        pdr = node->pdr;
        pdi = pdr->pdi;
        
        if (!pdi) {
            printk("GTP5G: %s - pdi is NULL, skip\n", __func__);
            continue;
        }

        if (pdi->sdf) {
            if (!sdf_filter_match(pdi->sdf, skb, hdrlen, GTP5G_SDF_FILTER_OUT)) {
                printk("GTP5G: %s - sdf_filter_match failed, continue\n", __func__);
                continue;
            }
        }

        printk("GTP5G: %s - PDR found! PDR ID=%u\n", __func__, pdr->id);
        return pdr;
    }

    printk("GTP5G: %s - no matching PDR found in hash bucket\n", __func__);
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
        printk("GTP5G: pdr_update_hlist_table - processing %u framed route nodes for PDR ID=%u\n",
               pdi->framed_route_num, pdr->id);

        for (j = 0; j < pdi->framed_route_num; j++) {
            struct framed_route_node *fr_node = pdi->framed_route_nodes[j];

            if (!fr_node) {
                printk("GTP5G: framed_route_node[%d] is NULL, skip\n", j);
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

            printk("GTP5G: added framed_route_node to hash table - PDR ID=%u, network_addr=%pI4\n",
                   pdr->id, &fr_node->network_addr);
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
