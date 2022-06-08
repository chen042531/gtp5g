#include <net/rtnetlink.h>
#include <net/ip.h>
#include <net/udp.h>

#include "dev.h"
#include "link.h"

const struct nla_policy gtp5g_policy[IFLA_GTP5G_MAX + 1] = {
    [IFLA_GTP5G_FD1]             = { .type = NLA_U32 },
    [IFLA_GTP5G_PDR_HASHSIZE]    = { .type = NLA_U32 },
    [IFLA_GTP5G_ROLE]            = { .type = NLA_U32 },
};
/* According to 3GPP TS 29.060. */
struct gtpv1_hdr {
	__u8	flags;
#define GTPV1_HDR_FLG_NPDU		0x01
#define GTPV1_HDR_FLG_SEQ		0x02
#define GTPV1_HDR_FLG_EXTHDR	0x04
#define GTPV1_HDR_FLG_MASK		0x07

	__u8	type;
	__be16	length;
	__be32	tid;
} __attribute__((packed)) gtpv1_hdr_t;

static void gtp5g_link_setup(struct net_device *dev)
{
    printk("<%s: %d> start\n", __func__, __LINE__);

    dev->netdev_ops = &gtp5g_netdev_ops;
    dev->needs_free_netdev = true;

    dev->hard_header_len = 0;
    dev->addr_len = 0;
    dev->mtu = ETH_DATA_LEN -
        (sizeof(struct iphdr) +
         sizeof(struct udphdr) +
         sizeof(struct gtpv1_hdr));

    /* Zero header length. */
    dev->type = ARPHRD_NONE;
    dev->flags = IFF_POINTOPOINT | IFF_NOARP | IFF_MULTICAST;

    dev->priv_flags |= IFF_NO_QUEUE;
    dev->features |= NETIF_F_LLTX;
    netif_keep_dst(dev);

    /* TODO: Modify the headroom size based on
     * what are the extension header going to support
     * */
    dev->needed_headroom = LL_MAX_HEADER +
        sizeof(struct iphdr) +
        sizeof(struct udphdr) +
        sizeof(struct gtpv1_hdr);
}

static int gtp5g_validate(struct nlattr *tb[], struct nlattr *data[],
    struct netlink_ext_ack *extack)
{
    int i;

	printk("<%s: %d> start\n", __func__, __LINE__);
	printk("<%s: %d> tb: %p\n", __func__, __LINE__, tb);
	printk("<%s: %d> data: %p\n", __func__, __LINE__, data);

	if (!data)
		return -EINVAL;

	for (i = 0; tb[i] != NULL; i++) {
		printk("<%s: %d> tb[%d]: type: %u\n", __func__, __LINE__, 0,
				tb[i]->nla_type);
		printk("<%s: %d> tb[%d]: len: %u\n", __func__, __LINE__, 0,
				tb[i]->nla_len);
	}
	for (i = 0; data[i] != NULL; i++) {
		printk("<%s: %d> data[%d]: type: %u\n", __func__, __LINE__, 0,
				data[i]->nla_type);
		printk("<%s: %d> data[%d]: len: %u\n", __func__, __LINE__, 0,
				data[i]->nla_len);
	}

	printk("<%s: %d> end\n", __func__, __LINE__);

    return 0;
}

static int gtp5g_newlink(struct net *src_net, struct net_device *dev,
    struct nlattr *tb[], struct nlattr *data[],
    struct netlink_ext_ack *extack)
{
    int err;

    printk("<%s: %d> start\n", __func__, __LINE__);
    
    err = register_netdevice(dev);
	if (err < 0) {
		netdev_dbg(dev, "failed to register new netdev %d\n", err);
		return err;
	}

	return 0;
}

static void gtp5g_dellink(struct net_device *dev, struct list_head *head)
{
    printk("<%s: %d> start\n", __func__, __LINE__);
    unregister_netdevice_queue(dev, head);
}

static size_t gtp5g_get_size(const struct net_device *dev)
{
    printk("<%s: %d> start\n", __func__, __LINE__);
	return 0;
}

static int gtp5g_fill_info(struct sk_buff *skb, const struct net_device *dev)
{
    printk("<%s: %d> start\n", __func__, __LINE__);
	return 0;
}

struct rtnl_link_ops gtp5g_link_ops __read_mostly = {
    .kind         = "gtp5g",
    .maxtype      = IFLA_GTP5G_MAX,
    .policy       = gtp5g_policy,
    .priv_size    = sizeof(struct gtp5g_dev),
    .setup        = gtp5g_link_setup,
    .validate     = gtp5g_validate,
    .newlink      = gtp5g_newlink,
    .dellink      = gtp5g_dellink,
    .get_size     = gtp5g_get_size,
    .fill_info    = gtp5g_fill_info,
};