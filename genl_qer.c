#include <net/genetlink.h>
#include <net/sock.h>

#include "dev.h"
#include "genl.h"
#include "genl_qer.h"
#include "qer.h"


int gtp5g_genl_add_qer(struct sk_buff *skb, struct genl_info *info)
{
    printk("<%s:%d> start\n", __func__, __LINE__);

	return 0;
}

int gtp5g_genl_del_qer(struct sk_buff *skb, struct genl_info *info)
{
    printk("<%s:%d> start\n", __func__, __LINE__);

	return 0;
}

int gtp5g_genl_get_qer(struct sk_buff *skb, struct genl_info *info)
{
    printk("<%s:%d> start\n", __func__, __LINE__);

	return 0;
}

int gtp5g_genl_dump_qer(struct sk_buff *skb, struct netlink_callback *cb)
{
    printk("<%s:%d> start\n", __func__, __LINE__);

	return 0;
}
