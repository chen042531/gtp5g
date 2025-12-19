#ifndef __UTIL_H__
#define __UTIL_H__

#include <linux/skbuff.h>

struct pdr;
struct far;

void ip_string(char *, int);
u32 gtp5g_get_skb_routing_mark(struct sk_buff *skb);
void debug_print_pdr(struct pdr *pdr);
void debug_print_far(struct far *far);

#endif // __UTIL_H__
