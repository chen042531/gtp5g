#ifndef GTP5G_DEV_H__
#define GTP5G_DEV_H__

#include <linux/netdevice.h>
#include <linux/rculist.h>

struct gtp5g_dev {
	struct list_head list;
};

extern const struct net_device_ops gtp5g_netdev_ops;

extern struct gtp5g_dev *gtp5g_find_dev(struct net *, int, int);

#endif