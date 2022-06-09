#include "dev.h"
#include "far.h"
#include "pdr.h"
#include "seid.h"
#include "hash.h"
#include "genl_far.h"

int gtp5g_genl_add_far(struct sk_buff *skb, struct genl_info *info)
{
    printk("<%s:%d> start\n", __func__, __LINE__);

	return 0;
}  

int gtp5g_genl_del_far(struct sk_buff *skb, struct genl_info *info)
{
    printk("<%s:%d> start\n", __func__, __LINE__);

	return 0;
}   

int gtp5g_genl_get_far(struct sk_buff *skb, struct genl_info *info)
{
    printk("<%s:%d> start\n", __func__, __LINE__);

	return 0;
}

int gtp5g_genl_dump_far(struct sk_buff *skb, struct netlink_callback *cb)
{
    printk("<%s:%d> start\n", __func__, __LINE__);

	return 0;
}