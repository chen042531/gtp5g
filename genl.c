#include <linux/module.h>
#include <net/genetlink.h>

#include "genl.h"

static int parse_f_teid(struct nlattr *a)
{
	struct nlattr *attrs[GTP5G_F_TEID_ATTR_MAX + 1];
	u32 teid;
	u32 gtpu_addr;
	int err;

	printk("<%s:%d> start\n", __func__, __LINE__);

	err = nla_parse_nested(attrs, GTP5G_F_TEID_ATTR_MAX, a, NULL, NULL);
	if (err != 0) {
		return err;
	}

	if (attrs[GTP5G_F_TEID_I_TEID]) {
		teid = htonl(nla_get_u32(attrs[GTP5G_F_TEID_I_TEID]));
		printk("TEID: %u\n", teid);
	}

	if (attrs[GTP5G_F_TEID_GTPU_ADDR_IPV4]) {
		gtpu_addr = nla_get_be32(attrs[GTP5G_F_TEID_GTPU_ADDR_IPV4]);
		printk("GTP-U Addr: %08x\n", gtpu_addr);
	}

	return 0;
}

static int parse_sdf_filter(struct nlattr *a)
{
	printk("<%s:%d> start\n", __func__, __LINE__);

	return 0;
}

static int parse_pdi(struct nlattr *a)
{
	struct nlattr *attrs[GTP5G_PDI_ATTR_MAX + 1];
	u32 ue_addr;
	int err;

	printk("<%s:%d> start\n", __func__, __LINE__);

	err = nla_parse_nested(attrs, GTP5G_PDI_ATTR_MAX, a, NULL, NULL);
	if (err != 0) {
		return err;
	}

	if (attrs[GTP5G_PDI_UE_ADDR_IPV4]) {
		ue_addr = nla_get_be32(attrs[GTP5G_PDI_UE_ADDR_IPV4]);
		printk("UE Addr: %08x\n", ue_addr);
	}

	if (attrs[GTP5G_PDI_F_TEID]) {
		parse_f_teid(attrs[GTP5G_PDI_F_TEID]);
	}

	if (attrs[GTP5G_PDI_SDF_FILTER]) {
		parse_sdf_filter(attrs[GTP5G_PDI_SDF_FILTER]);
	}

	return 0;
}

static int gtp5g_genl_add_pdr(struct sk_buff *skb, struct genl_info *info)
{
    int ifindex;
	int netnsfd;
	u32 pdr_id;
	u32 precedence;
	u8 removal;
	u32 far_id;
	u32 qer_id;

	printk("<%s:%d> start\n", __func__, __LINE__);

	printk("info.net: %p\n", genl_info_net(info));
	printk("info.nlhdr: %p\n", info->nlhdr);
	printk("info.snd_portid: %u\n", info->snd_portid);

    if (info->attrs[GTP5G_LINK]) {
		ifindex = nla_get_u32(info->attrs[GTP5G_LINK]);
	} else {
		ifindex = -1;
	}
    printk("ifindex: %d\n", ifindex);

    if (info->attrs[GTP5G_NET_NS_FD]) {
		netnsfd = nla_get_u32(info->attrs[GTP5G_NET_NS_FD]);
	} else {
		netnsfd = -1;
	}
	printk("netnsfd: %d\n", netnsfd);

	if (info->attrs[GTP5G_PDR_ID]) {
		pdr_id = nla_get_u32(info->attrs[GTP5G_PDR_ID]);
		printk("PDR ID: %u\n", pdr_id);
	}

	if (info->attrs[GTP5G_PDR_PRECEDENCE]) {
		precedence = nla_get_u32(info->attrs[GTP5G_PDR_PRECEDENCE]);
		printk("precedence: %u\n", precedence);
	}

	if (info->attrs[GTP5G_PDR_PDI]) {
		parse_pdi(info->attrs[GTP5G_PDR_PDI]);
	}

	if (info->attrs[GTP5G_OUTER_HEADER_REMOVAL]) {
		removal = nla_get_u8(info->attrs[GTP5G_OUTER_HEADER_REMOVAL]);
		printk("removal: %u\n", removal);
	}

	if (info->attrs[GTP5G_PDR_FAR_ID]) {
		far_id = nla_get_u32(info->attrs[GTP5G_PDR_FAR_ID]);
		printk("far_id: %u\n", far_id);
	}

	if (info->attrs[GTP5G_PDR_QER_ID]) {
		qer_id = nla_get_u32(info->attrs[GTP5G_PDR_QER_ID]);
		printk("qer_id: %u\n", qer_id);
	}

	return 0;
}

static int gtp5g_genl_del_pdr(struct sk_buff *skb, struct genl_info *info)
{
    printk("<%s:%d> start\n", __func__, __LINE__);

	return 0;
}  

static int gtp5g_genl_get_pdr(struct sk_buff *skb, struct genl_info *info)
{
    printk("<%s:%d> start\n", __func__, __LINE__);

	return 0;
}  

static int gtp5g_genl_dump_pdr(struct sk_buff *skb, struct netlink_callback *cb)
{
    printk("<%s:%d> start\n", __func__, __LINE__);

	return 0;
}  

static int gtp5g_genl_add_far(struct sk_buff *skb, struct genl_info *info)
{
    printk("<%s:%d> start\n", __func__, __LINE__);

	return 0;
}  

static int gtp5g_genl_del_far(struct sk_buff *skb, struct genl_info *info)
{
    printk("<%s:%d> start\n", __func__, __LINE__);

	return 0;
}   

static int gtp5g_genl_get_far(struct sk_buff *skb, struct genl_info *info)
{
    printk("<%s:%d> start\n", __func__, __LINE__);

	return 0;
}

static int gtp5g_genl_dump_far(struct sk_buff *skb, struct netlink_callback *cb)
{
    printk("<%s:%d> start\n", __func__, __LINE__);

	return 0;
}

static int gtp5g_genl_add_qer(struct sk_buff *skb, struct genl_info *info)
{
    printk("<%s:%d> start\n", __func__, __LINE__);

	return 0;
}

static int gtp5g_genl_del_qer(struct sk_buff *skb, struct genl_info *info)
{
    printk("<%s:%d> start\n", __func__, __LINE__);

	return 0;
}

static int gtp5g_genl_get_qer(struct sk_buff *skb, struct genl_info *info)
{
    printk("<%s:%d> start\n", __func__, __LINE__);

	return 0;
}

static int gtp5g_genl_dump_qer(struct sk_buff *skb, struct netlink_callback *cb)
{
    printk("<%s:%d> start\n", __func__, __LINE__);

	return 0;
}

static int gtp5g_genl_add_urr(struct sk_buff *skb, struct genl_info *info)
{
    /* Not implemented yet */
    return -1; 
}

static int gtp5g_genl_del_urr(struct sk_buff *skb, struct genl_info *info)
{
    /* Not implemented yet */
    return -1; 
}

static int gtp5g_genl_get_urr(struct sk_buff *skb, struct genl_info *info)
{
    /* Not implemented yet */
    return -1; 
}

static int gtp5g_genl_dump_urr(struct sk_buff *skb, struct netlink_callback *cb)
{
    /* Not implemented yet */
    return -1; 
}

static int gtp5g_genl_add_bar(struct sk_buff *skb, struct genl_info *info)
{
    /* Not implemented yet */
    return -1; 
}

static int gtp5g_genl_del_bar(struct sk_buff *skb, struct genl_info *info)
{
    /* Not implemented yet */
    return -1; 
}

static int gtp5g_genl_get_bar(struct sk_buff *skb, struct genl_info *info)
{
    /* Not implemented yet */
    return -1; 
}

static int gtp5g_genl_dump_bar(struct sk_buff *skb, struct netlink_callback *cb)
{
    /* Not implemented yet */
    return -1; 
}

static const struct nla_policy gtp5g_genl_pdr_policy[GTP5G_PDR_ATTR_MAX + 1] = {
    [GTP5G_PDR_ID]                              = { .type = NLA_U32, },
    [GTP5G_PDR_PRECEDENCE]                      = { .type = NLA_U32, },
    [GTP5G_PDR_PDI]                             = { .type = NLA_NESTED, },
    [GTP5G_OUTER_HEADER_REMOVAL]                = { .type = NLA_U8, },
    [GTP5G_PDR_FAR_ID]                          = { .type = NLA_U32, },
    [GTP5G_PDR_QER_ID]                          = { .type = NLA_U32, },
};

static const struct nla_policy gtp5g_genl_far_policy[GTP5G_FAR_ATTR_MAX + 1] = {
    [GTP5G_FAR_ID]                              = { .type = NLA_U32, },
    [GTP5G_FAR_APPLY_ACTION]                    = { .type = NLA_U8, },
    [GTP5G_FAR_FORWARDING_PARAMETER]            = { .type = NLA_NESTED, },
};

static const struct nla_policy gtp5g_genl_qer_policy[GTP5G_QER_ATTR_MAX + 1] = {
    [GTP5G_QER_ID]                              = { .type = NLA_U32, },
    [GTP5G_QER_GATE]                            = { .type = NLA_U8, },
    [GTP5G_QER_MBR]                             = { .type = NLA_NESTED, },
    [GTP5G_QER_GBR]                             = { .type = NLA_NESTED, },
    [GTP5G_QER_CORR_ID]                         = { .type = NLA_U32, },
    [GTP5G_QER_RQI]                             = { .type = NLA_U8, },
    [GTP5G_QER_QFI]                             = { .type = NLA_U8, },
    [GTP5G_QER_PPI]                             = { .type = NLA_U8, },
    [GTP5G_QER_RCSR]                            = { .type = NLA_U8, },
};

static const struct genl_ops gtp5g_genl_ops[] = {
    {
        .cmd = GTP5G_CMD_ADD_PDR,
        // .validate = GENL_DONT_VALIDATE_STRICT | GENL_DONT_VALIDATE_DUMP,
        .doit = gtp5g_genl_add_pdr,
        // .policy = gtp5g_genl_pdr_policy,
        .flags = GENL_ADMIN_PERM,
    },
    {
        .cmd = GTP5G_CMD_DEL_PDR,
        // .validate = GENL_DONT_VALIDATE_STRICT | GENL_DONT_VALIDATE_DUMP,
        .doit = gtp5g_genl_del_pdr,
        // .policy = gtp5g_genl_pdr_policy,
        .flags = GENL_ADMIN_PERM,
    },
    {
        .cmd = GTP5G_CMD_GET_PDR,
        // .validate = GENL_DONT_VALIDATE_STRICT | GENL_DONT_VALIDATE_DUMP,
        .doit = gtp5g_genl_get_pdr,
        .dumpit = gtp5g_genl_dump_pdr,
        // .policy = gtp5g_genl_pdr_policy,
        .flags = GENL_ADMIN_PERM,
    },
    {
        .cmd = GTP5G_CMD_ADD_FAR,
        // .validate = GENL_DONT_VALIDATE_STRICT | GENL_DONT_VALIDATE_DUMP,
        .doit = gtp5g_genl_add_far,
        // .policy = gtp5g_genl_far_policy,
        .flags = GENL_ADMIN_PERM,
    },
    {
        .cmd = GTP5G_CMD_DEL_FAR,
        // .validate = GENL_DONT_VALIDATE_STRICT | GENL_DONT_VALIDATE_DUMP,
        .doit = gtp5g_genl_del_far,
        // .policy = gtp5g_genl_far_policy,
        .flags = GENL_ADMIN_PERM,
    },
    {
        .cmd = GTP5G_CMD_GET_FAR,
        // .validate = GENL_DONT_VALIDATE_STRICT | GENL_DONT_VALIDATE_DUMP,
        .doit = gtp5g_genl_get_far,
        .dumpit = gtp5g_genl_dump_far,
        // .policy = gtp5g_genl_far_policy,
        .flags = GENL_ADMIN_PERM,
    },
    {
        .cmd = GTP5G_CMD_ADD_QER,
        // .validate = GENL_DONT_VALIDATE_STRICT | GENL_DONT_VALIDATE_DUMP,
        .doit = gtp5g_genl_add_qer,
        // .policy = gtp5g_genl_qer_policy,
        .flags = GENL_ADMIN_PERM,
    },
    {
        .cmd = GTP5G_CMD_DEL_QER,
        // .validate = GENL_DONT_VALIDATE_STRICT | GENL_DONT_VALIDATE_DUMP,
        .doit = gtp5g_genl_del_qer,
        // .policy = gtp5g_genl_qer_policy,
        .flags = GENL_ADMIN_PERM,
    },
    {
        .cmd = GTP5G_CMD_GET_QER,
        // .validate = GENL_DONT_VALIDATE_STRICT | GENL_DONT_VALIDATE_DUMP,
        .doit = gtp5g_genl_get_qer,
        .dumpit = gtp5g_genl_dump_qer,
        // .policy = gtp5g_genl_qer_policy,
        .flags = GENL_ADMIN_PERM,
    },
    {
        .cmd = GTP5G_CMD_ADD_URR,
        .doit = gtp5g_genl_add_urr,
        .flags = GENL_ADMIN_PERM,
    },
    {
        .cmd = GTP5G_CMD_DEL_URR,
        .doit = gtp5g_genl_del_urr,
        .flags = GENL_ADMIN_PERM,
    },
    {
        .cmd = GTP5G_CMD_GET_URR,
        .doit = gtp5g_genl_get_urr,
        .dumpit = gtp5g_genl_dump_urr,
        .flags = GENL_ADMIN_PERM,
    },
    {
        .cmd = GTP5G_CMD_ADD_BAR,
        .doit = gtp5g_genl_add_bar,
        .flags = GENL_ADMIN_PERM,
    },
    {
        .cmd = GTP5G_CMD_DEL_BAR,
        .doit = gtp5g_genl_del_bar,
        .flags = GENL_ADMIN_PERM,
    },
    {
        .cmd = GTP5G_CMD_GET_BAR,
        .doit = gtp5g_genl_get_bar,
        .dumpit = gtp5g_genl_dump_bar,
        .flags = GENL_ADMIN_PERM,
    }, 
};

struct genl_family gtp5g_genl_family __ro_after_init = {
    .name       = "gtp5g",
    .version    = 0,
    .hdrsize    = 0,
    .maxattr    = GTP5G_ATTR_MAX,
    .netnsok    = true,
    .module     = THIS_MODULE,
    .ops        = gtp5g_genl_ops,
    .n_ops      = ARRAY_SIZE(gtp5g_genl_ops),
};