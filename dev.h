#ifndef GTP5G_DEV_H__
#define GTP5G_DEV_H__

#include <linux/netdevice.h>
#include <linux/rculist.h>
#include <linux/socket.h>

struct gtp5g_dev {
	struct list_head list;
	struct sock *sk1u;
	struct net_device *dev;
	unsigned int role;
	unsigned int hash_size;
	struct hlist_head *pdr_id_hash;
	struct hlist_head *far_id_hash;
	struct hlist_head *qer_id_hash;
	struct hlist_head *i_teid_hash;
	struct hlist_head *addr_hash;
	struct hlist_head *related_far_hash;
	struct hlist_head *related_qer_hash;
};

extern const struct net_device_ops gtp5g_netdev_ops;

extern struct gtp5g_dev *gtp5g_find_dev(struct net *, int, int);

extern int dev_hashtable_new(struct upf_dev *, int);
extern void dev_hashtable_free(struct upf_dev *);

#endif