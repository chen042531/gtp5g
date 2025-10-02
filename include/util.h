#ifndef __UTIL_H__
#define __UTIL_H__

#include <linux/skbuff.h>

void ip_string(char *, int);
u32 gtp5g_get_skb_routing_mark(struct sk_buff *skb);
u32 gtp5g_get_skb_routing_mark_with_info(struct sk_buff *skb, const char *context);

#endif // __UTIL_H__
