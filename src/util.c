#include <linux/kernel.h>
#include <linux/skbuff.h>

#include "util.h"

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
    printk(KERN_INFO "GTP5G: %s - SKB mark: 0x%x (%u)\n", __func__, skb->mark, skb->mark);
    
    /* Directly return the mark field from SKB */
    return skb->mark;
}