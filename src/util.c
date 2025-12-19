#include <linux/kernel.h>
#include <linux/skbuff.h>

#include "util.h"
#include "pdr.h"
#include "far.h"

void ip_string(char * ip_str, int ip_int){
    sprintf(ip_str, "%i.%i.%i.%i",
          (ip_int) & 0xFF,
          (ip_int >> 8) & 0xFF,
          (ip_int >> 16) & 0xFF,
          (ip_int >> 24) & 0xFF);
}

/**
 * gtp5g_get_skb_routing_mark - Get routing rule mark from SKB
 * @skb: socket buffer pointer
 *
 * This function reads the mark value set by routing rules or iptables rules
 * from the SKB. The mark value is typically used for routing policies,
 * traffic classification, or other network processing logic.
 *
 * Return: SKB mark value (u32)
 */
u32 gtp5g_get_skb_routing_mark(struct sk_buff *skb)
{
    if (!skb) {
        /* Return 0 if SKB is NULL */
        return 0;
    }
    /* Directly return the mark field from SKB */
    return skb->mark;
}

/**
 * debug_print_pdr - Print PDR (Packet Detection Rule) contents
 * @pdr: pointer to struct pdr
 */
void debug_print_pdr(struct pdr *pdr)
{
    if (!pdr) {
        printk(KERN_INFO "[GTP5G] PDR is NULL\n");
        return;
    }

    printk(KERN_INFO "[GTP5G] ===== PDR DEBUG INFO =====\n");
    printk(KERN_INFO "[GTP5G] PDR ID: %u\n", pdr->id);
    printk(KERN_INFO "[GTP5G] PDR SEID: %llu\n", pdr->seid);
    printk(KERN_INFO "[GTP5G] PDR Precedence: %u\n", pdr->precedence);

    if (pdr->pdi) {
        printk(KERN_INFO "[GTP5G] PDR SrcIntf: %u (0=ACCESS, 1=CORE)\n", pdr->pdi->srcIntf);

        if (pdr->pdi->ue_addr_ipv4) {
            printk(KERN_INFO "[GTP5G] PDR UE IPv4: %pI4\n", pdr->pdi->ue_addr_ipv4);
        }

        if (pdr->pdi->f_teid) {
            printk(KERN_INFO "[GTP5G] PDR F-TEID: 0x%x, GTP-U IP: %pI4\n",
                   pdr->pdi->f_teid->teid, &pdr->pdi->f_teid->gtpu_addr_ipv4);
        }
    }

    printk(KERN_INFO "[GTP5G] PDR QFI: %u\n", pdr->qfi);
    printk(KERN_INFO "[GTP5G] PDR UL/DL Gate: 0x%x\n", pdr->ul_dl_gate);

    if (pdr->far_id) {
        printk(KERN_INFO "[GTP5G] PDR FAR ID: %u\n", *pdr->far_id);
    }

    printk(KERN_INFO "[GTP5G] PDR QER Count: %u\n", pdr->qer_num);
    printk(KERN_INFO "[GTP5G] PDR URR Count: %u\n", pdr->urr_num);

    printk(KERN_INFO "[GTP5G] PDR UL Drop Count: %llu\n", pdr->ul_drop_cnt);
    printk(KERN_INFO "[GTP5G] PDR DL Drop Count: %llu\n", pdr->dl_drop_cnt);

    printk(KERN_INFO "[GTP5G] PDR UL Packets: %llu, Bytes: %llu\n",
           pdr->ul_pkt_cnt, pdr->ul_byte_cnt);
    printk(KERN_INFO "[GTP5G] PDR DL Packets: %llu, Bytes: %llu\n",
           pdr->dl_pkt_cnt, pdr->dl_byte_cnt);

    printk(KERN_INFO "[GTP5G] ===== PDR DEBUG END =====\n");
}

/**
 * debug_print_far - Print FAR (Forwarding Action Rule) contents
 * @far: pointer to struct far
 */
void debug_print_far(struct far *far)
{
    if (!far) {
        printk(KERN_INFO "[GTP5G] FAR is NULL\n");
        return;
    }

    printk(KERN_INFO "[GTP5G] ===== FAR DEBUG INFO =====\n");
    printk(KERN_INFO "[GTP5G] FAR ID: %u\n", far->id);
    printk(KERN_INFO "[GTP5G] FAR SEID: %llu\n", far->seid);

    printk(KERN_INFO "[GTP5G] FAR Action: 0x%x", far->action);
    if (far->action & FAR_ACTION_DROP)
        printk(KERN_CONT " DROP");
    if (far->action & FAR_ACTION_FORW)
        printk(KERN_CONT " FORW");
    if (far->action & FAR_ACTION_BUFF)
        printk(KERN_CONT " BUFF");
    if (far->action & FAR_ACTION_NOCP)
        printk(KERN_CONT " NOCP");
    if (far->action & FAR_ACTION_DUPL)
        printk(KERN_CONT " DUPL");
    printk(KERN_CONT "\n");

    printk(KERN_INFO "[GTP5G] FAR Sequence Number: %u\n", far->seq_number);

    if (far->bar_id) {
        printk(KERN_INFO "[GTP5G] FAR BAR ID: %u\n", *far->bar_id);
    }

    if (far->fwd_param) {
        struct forwarding_parameter *fwd_param = rcu_dereference(far->fwd_param);
        if (fwd_param) {
            if (fwd_param->hdr_creation) {
                struct outer_header_creation *hdr = fwd_param->hdr_creation;
                printk(KERN_INFO "[GTP5G] FAR Outer Header TEID: 0x%x\n", hdr->teid);
                printk(KERN_INFO "[GTP5G] FAR Outer Header Peer IP: %pI4\n",
                       &hdr->peer_addr_ipv4);
                printk(KERN_INFO "[GTP5G] FAR Outer Header Port: %u\n", hdr->port);
                printk(KERN_INFO "[GTP5G] FAR Outer Header ToS/TC: %u\n", hdr->tosTc);
            }

            if (fwd_param->fwd_policy) {
                printk(KERN_INFO "[GTP5G] FAR Forwarding Policy: %s\n",
                       fwd_param->fwd_policy->identifier);
                printk(KERN_INFO "[GTP5G] FAR Forwarding Policy Mark: %u\n",
                       fwd_param->fwd_policy->mark);
            }
        }
    }

    printk(KERN_INFO "[GTP5G] ===== FAR DEBUG END =====\n");
}