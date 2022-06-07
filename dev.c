#include <linux/netdevice.h>
#include <linux/version.h>

#include "dev.h"
#include "genl.h"


struct gtp5g_dev *gtp5g_find_dev(struct net *src_net, int ifindex, int netnsfd)
{
    struct gtp5g_dev *gtp = NULL;
    struct net_device *dev;
    struct net *net;

    /* Examine the link attributes and figure out which network namespace
     * we are talking about.
     */
    if (netnsfd == -1)
		net = get_net(src_net);
	else
		net = get_net_ns_by_fd(netnsfd);

    if (IS_ERR(net))
        return NULL;

    /* Check if there's an existing gtp5g device to configure */
    dev = dev_get_by_index_rcu(net, ifindex);
    if (dev && dev->netdev_ops == &gtp5g_netdev_ops)
        gtp = netdev_priv(dev);
    else
		gtp = NULL;

    put_net(net);

    return gtp;
}

static int gtp5g_dev_init(struct net_device *dev)
{
    printk("<%s: %d> start\n", __func__, __LINE__);

    dev->tstats = netdev_alloc_pcpu_stats(struct pcpu_sw_netstats);
    if (!dev->tstats) {
        return -ENOMEM;
    }

    return 0;
}

static void gtp5g_dev_uninit(struct net_device *dev)
{
    printk("<%s: %d> start\n", __func__, __LINE__);
    free_percpu(dev->tstats);
}

/**
 * Entry function for Downlink packets
 * */
static netdev_tx_t gtp5g_dev_xmit(struct sk_buff *skb, struct net_device *dev)
{
    printk("<%s: %d> start\n", __func__, __LINE__);
    
    // skb_dump("packet:", skb, 1);

	dev_kfree_skb(skb);
	return NETDEV_TX_OK;
}

const struct net_device_ops gtp5g_netdev_ops = {
    .ndo_init           = gtp5g_dev_init,
    .ndo_uninit         = gtp5g_dev_uninit,
    .ndo_start_xmit     = gtp5g_dev_xmit,
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 11, 0)
    .ndo_get_stats64    = dev_get_tstats64,
#else
    .ndo_get_stats64    = ip_tunnel_get_stats64,
#endif
};
