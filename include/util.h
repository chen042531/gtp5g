#ifndef __UTIL_H__
#define __UTIL_H__

#include <linux/skbuff.h>
#include <net/flow.h>
#include <net/route.h>
#include <net/sock.h>
#include <net/inet_sock.h>

void ip_string(char *, int);
u32 get_skb_routing_mark(struct sk_buff *skb);

/**
 * set_flowi4_dsfield - Fill in the DS field of a route lookup key from a socket
 * @fl4: The flow key being built, already zeroed by the caller
 * @sk: The socket whose IP_TOS the outer packet inherits
 * @localroute: Honour SOCK_LOCALROUTE (SO_DONTROUTE) on @sk
 *
 * Kernels that renamed flowi4_tos to flowi4_dscp also stopped smuggling the
 * SOCK_LOCALROUTE hint through the low bit of the DS field, expressing it as a
 * link-scoped lookup instead. flowi4_dscp is a macro, so it doubles as the
 * feature test.
 *
 * The new branch deliberately avoids inet_sk_dscp() and ip_sock_rt_scope(),
 * which appeared in their own releases. inet_dsfield_to_dscp() and RT_SCOPE_*
 * are instead guaranteed to be present: flowic_dscp is typed dscp_t, so
 * net/inet_dscp.h necessarily exists wherever flowi4_dscp does.
 */
static inline void set_flowi4_dsfield(struct flowi4 *fl4,
    const struct sock *sk, bool localroute)
{
#ifdef flowi4_dscp
    fl4->flowi4_dscp = inet_dsfield_to_dscp(READ_ONCE(inet_sk(sk)->tos));
    if (localroute)
        fl4->flowi4_scope = sock_flag(sk, SOCK_LOCALROUTE) ?
            RT_SCOPE_LINK : RT_SCOPE_UNIVERSE;
#else
    fl4->flowi4_tos = RT_TOS(inet_sk(sk)->tos);
    if (localroute)
        fl4->flowi4_tos |= sock_flag(sk, SOCK_LOCALROUTE);
#endif
}

/*
 * ============================================================================
 * Framed Route Mark Definitions
 * ============================================================================
 *
 * The skb->mark field can be used by other system components (iptables, tc,
 * routing rules). To avoid conflicts, we use a specific bit field format:
 *
 *   Bit 31 (0x80000000): Flag indicating this is a framed route mark
 *   Bits 0-5 (0x0000003F): Prefix length (0-32, only needs 6 bits)
 *   Bits 6-30: Reserved for future use
 *
 * When setting a framed route mark in routing rules, use:
 *   mark = FRAMED_ROUTE_MARK_FLAG | prefix_length
 *
 * Example: For prefix /24, set mark to 0x80000018 (0x80000000 | 24)
 */

#define FRAMED_ROUTE_MARK_FLAG   0x80000000  /* Highest bit as flag */
#define FRAMED_ROUTE_PREFIX_MASK 0x0000003F  /* Prefix length: 6 bits (0-63, only 0-32 valid) */

/**
 * is_framed_route_mark - Check if mark is a framed route mark
 * @mark: The skb mark value to check
 *
 * Return: true if the mark has the framed route flag set, false otherwise
 */
static inline bool is_framed_route_mark(u32 mark)
{
    return (mark & FRAMED_ROUTE_MARK_FLAG) != 0;
}

/**
 * get_framed_route_prefix - Extract prefix length from framed route mark
 * @mark: The skb mark value (must have framed route flag set)
 *
 * Return: The prefix length (0-32)
 *
 * Note: Caller should verify is_framed_route_mark() first
 */
static inline u32 get_framed_route_prefix(u32 mark)
{
    u32 prefix = mark & FRAMED_ROUTE_PREFIX_MASK;

    /* Clamp to valid IPv4 prefix range */
    return (prefix > 32) ? 32 : prefix;
}

#endif // __UTIL_H__
