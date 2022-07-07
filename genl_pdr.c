#include <linux/module.h>
#include <linux/socket.h>
#include <linux/net.h>

#include <net/ip.h>
#include <net/genetlink.h>
#include <net/rtnetlink.h>

#include "dev.h"
#include "genl.h"
#include "genl_pdr.h"
#include "pdr.h"
#include "api_version.h"

#include <linux/rculist.h>
#include <net/netns/generic.h>
#include "net.h"
#include "util.h"

static int pdr_fill(struct pdr *, struct gtp5g_dev *, struct genl_info *);
static int parse_pdi(struct pdr *, struct nlattr *);
static int parse_f_teid(struct pdi *, struct nlattr *);
static int parse_sdf_filter(struct pdi *, struct nlattr *);
static int parse_ip_filter_rule(struct sdf_filter *, struct nlattr *);

static int gtp5g_genl_fill_rule(struct sk_buff *, struct ip_filter_rule *);
static int gtp5g_genl_fill_sdf(struct sk_buff *, struct sdf_filter *);
static int gtp5g_genl_fill_f_teid(struct sk_buff *, struct local_f_teid *);
static int gtp5g_genl_fill_pdi(struct sk_buff *, struct pdi *);
static int gtp5g_genl_fill_pdr(struct sk_buff *, u32, u32, u32, struct pdr *);

int gtp5g_genl_add_pdr(struct sk_buff *skb, struct genl_info *info)
{
    struct gtp5g_dev *gtp;
    struct pdr *pdr;
    int ifindex;
    int netnsfd;
    u64 seid;
    u16 pdr_id;
    int err;

    if (info->attrs[GTP5G_LINK]) {
        ifindex = nla_get_u32(info->attrs[GTP5G_LINK]);
    } else {
        ifindex = -1;
    }

    if (info->attrs[GTP5G_NET_NS_FD]) {
        netnsfd = nla_get_u32(info->attrs[GTP5G_NET_NS_FD]);
    } else {
        netnsfd = -1;
    }

    rtnl_lock();
    rcu_read_lock();

    gtp = gtp5g_find_dev(sock_net(skb->sk), ifindex, netnsfd);
    if (!gtp) {
        rcu_read_unlock();
        rtnl_unlock();
        return -ENODEV;
    }

    if (info->attrs[GTP5G_PDR_SEID]) {
        set_api_with_seid(true);
        seid = nla_get_u32(info->attrs[GTP5G_PDR_SEID]);
    } else {
        set_api_with_seid(false);
        seid = 0;
    }

    /* 
     * For backward compatability: 
     * If information has GTP5G_PDR_URR_ID, 
     * it means that user use the API which support 
     * URR and BAR feature. Otherwise, use the older API. 
     */
    if (info->attrs[GTP5G_PDR_URR_ID])
        set_api_with_urr_bar(true);
    else
        set_api_with_urr_bar(false);

    if (info->attrs[GTP5G_PDR_ID]) {
        pdr_id = nla_get_u32(info->attrs[GTP5G_PDR_ID]);
    } else {
        rcu_read_unlock();
        rtnl_unlock();
        return -ENODEV;
    }

    pdr = find_pdr_by_id(gtp, seid, pdr_id);
    if (pdr) {
        if (info->nlhdr->nlmsg_flags & NLM_F_EXCL) {
            rcu_read_unlock();
            rtnl_unlock();
            return -EEXIST;
        }
        if (!(info->nlhdr->nlmsg_flags & NLM_F_REPLACE)) {
            rcu_read_unlock();
            rtnl_unlock();
            return -EOPNOTSUPP;
        }

        err = pdr_fill(pdr, gtp, info);
        if (err) {
            pdr_context_delete(pdr);
            return err;
        }

        rcu_read_unlock();
        rtnl_unlock();
        return 0;
    }

    if (info->nlhdr->nlmsg_flags & NLM_F_REPLACE) {
        rcu_read_unlock();
        rtnl_unlock();
        return -ENOENT;
    }

    if (info->nlhdr->nlmsg_flags & NLM_F_APPEND) {
        rcu_read_unlock();
        rtnl_unlock();
        return -EOPNOTSUPP;
    }

    // Check only at the creation part
    if (!info->attrs[GTP5G_PDR_PRECEDENCE]) {
        rcu_read_unlock();
        rtnl_unlock();
        return -EINVAL;
    }

    pdr = kzalloc(sizeof(*pdr), GFP_ATOMIC);
    if (!pdr) {
        rcu_read_unlock();
        rtnl_unlock();
        return -ENOMEM;
    }

    sock_hold(gtp->sk1u);
    pdr->sk = gtp->sk1u;
    pdr->dev = gtp->dev;

    err = pdr_fill(pdr, gtp, info);
    if (err) {
        pdr_context_delete(pdr);
        rcu_read_unlock();
        rtnl_unlock();
        return err;
    }

    pdr_append(seid, pdr_id, pdr, gtp);

    rcu_read_unlock();
    rtnl_unlock();

    return 0;
}

int gtp5g_genl_del_pdr(struct sk_buff *skb, struct genl_info *info)
{
    struct gtp5g_dev *gtp;
    struct pdr *pdr;
    int ifindex;
    int netnsfd;
    u64 seid;
    u16 pdr_id;

    if (!info->attrs[GTP5G_LINK])
        return -EINVAL;
    ifindex = nla_get_u32(info->attrs[GTP5G_LINK]);

    if (info->attrs[GTP5G_NET_NS_FD])
        netnsfd = nla_get_u32(info->attrs[GTP5G_NET_NS_FD]);
    else
        netnsfd = -1;

    rcu_read_lock();

    gtp = gtp5g_find_dev(sock_net(skb->sk), ifindex, netnsfd);
    if (!gtp) {
        rcu_read_unlock();
        return -ENODEV;
    }

    if (info->attrs[GTP5G_PDR_SEID]) {
        seid = nla_get_u64(info->attrs[GTP5G_PDR_SEID]);
    } else {
        seid = 0;
    }

    if (info->attrs[GTP5G_PDR_ID]) {
        pdr_id = nla_get_u16(info->attrs[GTP5G_PDR_ID]);
    } else {
        rcu_read_unlock();
        return -ENODEV;
    }

    pdr = find_pdr_by_id(gtp, seid, pdr_id);
    if (!pdr) {
        rcu_read_unlock();
        return -ENOENT;
    }

    pdr_context_delete(pdr);
    rcu_read_unlock();

    return 0;
}  

int gtp5g_genl_get_pdr(struct sk_buff *skb, struct genl_info *info)
{
    struct gtp5g_dev *gtp;
    struct pdr *pdr;
    int ifindex;
    int netnsfd;
    u64 seid;
    u16 pdr_id;
    struct sk_buff *skb_ack;
    int err;

    if (!info->attrs[GTP5G_LINK])
        return -EINVAL;
    ifindex = nla_get_u32(info->attrs[GTP5G_LINK]);

    if (info->attrs[GTP5G_NET_NS_FD])
        netnsfd = nla_get_u32(info->attrs[GTP5G_NET_NS_FD]);
    else
        netnsfd = -1;

    rcu_read_lock();

    gtp = gtp5g_find_dev(sock_net(skb->sk), ifindex, netnsfd);
    if (!gtp) {
        rcu_read_unlock();
        return -ENODEV;
    }

    if (info->attrs[GTP5G_PDR_SEID]) {
        seid = nla_get_u64(info->attrs[GTP5G_PDR_SEID]);
    } else {
        seid = 0;
    }

    if (info->attrs[GTP5G_PDR_ID]) {
        pdr_id = nla_get_u16(info->attrs[GTP5G_PDR_ID]);
    } else {
        rcu_read_unlock();
        return -ENODEV;
    }

    pdr = find_pdr_by_id(gtp, seid, pdr_id);
    if (!pdr) {
        rcu_read_unlock();
        return -ENOENT;
    }

    skb_ack = genlmsg_new(NLMSG_GOODSIZE, GFP_ATOMIC);
    if (!skb_ack) {
        rcu_read_unlock();
        return -ENOMEM;
    }

    err = gtp5g_genl_fill_pdr(skb_ack,
            NETLINK_CB(skb).portid,
            info->snd_seq,
            info->nlhdr->nlmsg_type,
            pdr);
    if (err) {
        kfree_skb(skb_ack);
        rcu_read_unlock();
        return err;
    }

    rcu_read_unlock();

    return genlmsg_unicast(genl_info_net(info), skb_ack, info->snd_portid);
}  

int gtp5g_genl_dump_pdr(struct sk_buff *skb, struct netlink_callback *cb)
{
    /* netlink_callback->args
     * args[0] : index of gtp5g dev id
     * args[1] : index of gtp5g hash entry id in dev
     * args[2] : index of gtp5g pdr id
     * args[5] : set non-zero means it is finished
     */
    struct gtp5g_dev *gtp;
    struct gtp5g_dev *last_gtp = (struct gtp5g_dev *)cb->args[0];
    struct net *net = sock_net(skb->sk);
    struct gtp5g_net *gn = net_generic(net, GTP5G_NET_ID());
    int i;
    int last_hash_entry_id = cb->args[1];
    int ret;
    u16 pdr_id = cb->args[2];
    struct pdr *pdr;

    if (cb->args[5])
        return 0;

    list_for_each_entry_rcu(gtp, &gn->gtp5g_dev_list, list) {
        if (last_gtp && last_gtp != gtp)
            continue;
        else
            last_gtp = NULL;

        for (i = last_hash_entry_id; i < gtp->hash_size; i++) {
            hlist_for_each_entry_rcu(pdr, &gtp->pdr_id_hash[i], hlist_id) {
                if (pdr_id && pdr_id != pdr->id)
                    continue;
                else
                    pdr_id = 0;

                ret = gtp5g_genl_fill_pdr(skb,
                        NETLINK_CB(cb->skb).portid,
                        cb->nlh->nlmsg_seq,
                        cb->nlh->nlmsg_type,
                        pdr);
                if (ret) {
                    cb->args[0] = (unsigned long)gtp;
                    cb->args[1] = i;
                    cb->args[2] = pdr->id;
                    goto out;
                }
            }
        }
    }
    cb->args[5] = 1;
out:
    return skb->len;
}  


static int pdr_fill(struct pdr *pdr, struct gtp5g_dev *gtp, struct genl_info *info)
{
    char *str;
    int err;

    struct nlattr *hdr = nlmsg_attrdata(info->nlhdr, 0);
    int remaining = nlmsg_attrlen(info->nlhdr, 0);

    struct qer *tmp_q;
    struct rel_qer *rq;
    int i = 0;
    int n = 0;
    u32 *tmp;
    // LIST_HEAD(Head_Node);
    // pdr->rel_qer_list = &Head_Node;
    // INIT_LIST_HEAD(pdr->rel_qer_list);

    pdr->seid = 0;

    hdr = nla_next(hdr, &remaining);
    while (nla_ok(hdr, remaining)) {
        switch (nla_type(hdr)) {
            case GTP5G_PDR_SEID:
                pdr->seid = nla_get_u64(hdr);
                 printk(">>>>> pdr->seid:%llu\n", pdr->seid);
                break;
            case GTP5G_PDR_ID:
                pdr->id = nla_get_u16(hdr);
                 printk(">>>>> pdr_id:%u\n", pdr->id);
                break;
            case GTP5G_PDR_PRECEDENCE:
                pdr->precedence = nla_get_u32(hdr);
                printk(">>>>> pdr->precedence:%u\n", pdr->precedence);
                break;
            case GTP5G_OUTER_HEADER_REMOVAL:
                if (!pdr->outer_header_removal) {
                    pdr->outer_header_removal = kzalloc(sizeof(*pdr->outer_header_removal), GFP_ATOMIC);
                    if (!pdr->outer_header_removal)
                        return -ENOMEM;
                }
                *pdr->outer_header_removal = nla_get_u8(hdr);
                 printk(">>>>> outer_header_removal\n");
                break;
            case GTP5G_PDR_ROLE_ADDR_IPV4:
                /* Not in 3GPP spec, just used for routing */
                pdr->role_addr_ipv4.s_addr = nla_get_u32(hdr);
                printk(">>>>> pdr->role_addr_ipv4:%pI4\n", 
                    &pdr->role_addr_ipv4.s_addr);
                break;
            case GTP5G_PDR_UNIX_SOCKET_PATH:
                /* Not in 3GPP spec, just used for buffering */
                str = nla_data(hdr);
                pdr->addr_unix.sun_family = AF_UNIX;
                strncpy(pdr->addr_unix.sun_path, str, nla_len(hdr));
                 printk(">>>>> SOCKET_PATH:%s\n", 
                    pdr->addr_unix.sun_path);
                break;
            case GTP5G_PDR_FAR_ID:
                if (!pdr->far_id) {
                    pdr->far_id = kzalloc(sizeof(*pdr->far_id), GFP_ATOMIC);
                    if (!pdr->far_id)
                        return -ENOMEM;
                }
                *pdr->far_id = nla_get_u32(hdr);
                printk(">>>>> far_id:%u\n", *pdr->far_id);
                break;
            case GTP5G_PDR_QER_ID:
                printk("PDRid:%u QERid:%u", 
                        nla_get_u16(info->attrs[GTP5G_PDR_ID]),
                        nla_get_u32(hdr));
                tmp = kzalloc((++pdr->num_rel_qer) * QER_ID_SIZE, GFP_ATOMIC);
                if (!tmp)
                    return -ENOMEM;
                
                if (pdr->rel_qer_list) {
                    memcpy(tmp, pdr->rel_qer_list, pdr->num_rel_qer*QER_ID_SIZE);
                    kfree(pdr->rel_qer_list);
                }
                
                tmp[pdr->num_rel_qer-1] = nla_get_u32(hdr);
                printk("-----------:%u---> %u", nla_get_u32(hdr), tmp[pdr->num_rel_qer-1]);
                pdr->rel_qer_list = tmp;                
                break;
            case GTP5G_PDR_PDI:
                err = parse_pdi(pdr, hdr);
                if (err)
                    return err;
                printk(">>>>> parse_pdi\n");
                break;
        }
        // printk(">>> remaining:%u\n", remaining);
        // printk(">>> type:%u\n, len:%u\n", nla_type(hdr), nla_len(hdr));
        hdr = nla_next(hdr, &remaining);
    }
    // printk(">>>>>>>>>>>>>>>>>>>>>>&&& %u", pdr->num_rel_qer);
    for (i = 0; i < pdr->num_rel_qer; i++ )
    {   
        // printk("=====++++==-----> %llu", pdr->seid,pdr->rel_qer_list[i] );
        pdr->qer = find_qer_by_id(gtp, pdr->seid, pdr->rel_qer_list[i]);
        if ( pdr->qer->qfi){
            printk(">>>>>>>>>>>====== exit%u",pdr->qer->qfi);
             break;
        }else{
            printk(">>>>>>>>>>>====== not exit%u", pdr->qer->qfi);
        }
        // if (pdr->qer->qfi)
        //     break;
        printk("===+++++==seid:%llu, qerId:%u", pdr->seid,pdr->rel_qer_list[i]);
    }
    
    if (!pdr)
        return -EINVAL;

    pdr->af = AF_INET;

    pdr->far = find_far_by_id(gtp, pdr->seid, *pdr->far_id);
    far_set_pdr(pdr->seid, *pdr->far_id, &pdr->hlist_related_far, gtp);

    

    if (unix_sock_client_update(pdr) < 0)
        return -EINVAL;

    // Update hlist table
    pdr_update_hlist_table(pdr, gtp);

    return 0;
}

static int parse_pdi(struct pdr *pdr, struct nlattr *a)
{
    struct nlattr *attrs[GTP5G_PDI_ATTR_MAX + 1];
    struct pdi *pdi;
    int err;
    char ip_str[40];

    err = nla_parse_nested(attrs, GTP5G_PDI_ATTR_MAX, a, NULL, NULL);
    if (err)
        return err;

    if (!pdr->pdi) {
        pdr->pdi = kzalloc(sizeof(*pdr->pdi), GFP_ATOMIC);
        if (!pdr->pdi)
            return -ENOMEM;
    }
    pdi = pdr->pdi;

    if (attrs[GTP5G_PDI_UE_ADDR_IPV4]) {
        if (!pdi->ue_addr_ipv4) {
            pdi->ue_addr_ipv4 = kzalloc(sizeof(*pdi->ue_addr_ipv4), GFP_ATOMIC);
            if (!pdi->ue_addr_ipv4)
                return -ENOMEM;
        }
        pdi->ue_addr_ipv4->s_addr = nla_get_be32(attrs[GTP5G_PDI_UE_ADDR_IPV4]);

        ip_string(ip_str, pdi->ue_addr_ipv4->s_addr);
    }

    if (attrs[GTP5G_PDI_F_TEID]) {
        err = parse_f_teid(pdi, attrs[GTP5G_PDI_F_TEID]);
        if (err)
            return err;
    }

    if (attrs[GTP5G_PDI_SDF_FILTER]) {
        err = parse_sdf_filter(pdi, attrs[GTP5G_PDI_SDF_FILTER]);
        if (err)
            return err;
    }

    return 0;
}

static int parse_f_teid(struct pdi *pdi, struct nlattr *a)
{
    struct nlattr *attrs[GTP5G_F_TEID_ATTR_MAX + 1];
    struct local_f_teid *f_teid;
    int err;

    err = nla_parse_nested(attrs, GTP5G_F_TEID_ATTR_MAX, a, NULL, NULL);
    if (err)
        return err;

    if (!attrs[GTP5G_F_TEID_I_TEID])
        return -EINVAL;

    if (!attrs[GTP5G_F_TEID_GTPU_ADDR_IPV4])
        return -EINVAL;

    if (!pdi->f_teid) {
         pdi->f_teid = kzalloc(sizeof(*pdi->f_teid), GFP_ATOMIC);
         if (!pdi->f_teid)
             return -ENOMEM;
    }
    f_teid = pdi->f_teid;

    f_teid->teid = htonl(nla_get_u32(attrs[GTP5G_F_TEID_I_TEID]));

    f_teid->gtpu_addr_ipv4.s_addr = nla_get_be32(attrs[GTP5G_F_TEID_GTPU_ADDR_IPV4]);

    return 0;
}

static int parse_sdf_filter(struct pdi *pdi, struct nlattr *a)
{
    struct nlattr *attrs[GTP5G_SDF_FILTER_ATTR_MAX + 1];
    struct sdf_filter *sdf;
    int err;

    err = nla_parse_nested(attrs, GTP5G_SDF_FILTER_ATTR_MAX, a, NULL, NULL);
    if (err)
        return err;

    if (!pdi->sdf) {
        pdi->sdf = kzalloc(sizeof(*pdi->sdf), GFP_ATOMIC);
        if (!pdi->sdf)
            return -ENOMEM;
    }
    sdf = pdi->sdf;

    if (attrs[GTP5G_SDF_FILTER_FLOW_DESCRIPTION]) {
        err = parse_ip_filter_rule(sdf, attrs[GTP5G_SDF_FILTER_FLOW_DESCRIPTION]);
        if (err)
            return err;
    }

    if (attrs[GTP5G_SDF_FILTER_TOS_TRAFFIC_CLASS]) {
        if (!sdf->tos_traffic_class) {
            sdf->tos_traffic_class = kzalloc(sizeof(*sdf->tos_traffic_class), GFP_ATOMIC);
            if (!sdf->tos_traffic_class)
                return -ENOMEM;
        }
        *sdf->tos_traffic_class = nla_get_u16(attrs[GTP5G_SDF_FILTER_TOS_TRAFFIC_CLASS]);
    }

    if (attrs[GTP5G_SDF_FILTER_SECURITY_PARAMETER_INDEX]) {
        if (!sdf->security_param_idx) {
            sdf->security_param_idx = kzalloc(sizeof(*sdf->security_param_idx), GFP_ATOMIC);
            if (!sdf->security_param_idx)
                return -ENOMEM;
                }
        *sdf->security_param_idx = nla_get_u32(attrs[GTP5G_SDF_FILTER_SECURITY_PARAMETER_INDEX]);
    }

    if (attrs[GTP5G_SDF_FILTER_FLOW_LABEL]) {
        if (!sdf->flow_label) {
            sdf->flow_label = kzalloc(sizeof(*sdf->flow_label), GFP_ATOMIC);
            if (!sdf->flow_label)
                return -ENOMEM;
                }
        *sdf->flow_label = nla_get_u32(attrs[GTP5G_SDF_FILTER_FLOW_LABEL]);
    }

    if (attrs[GTP5G_SDF_FILTER_SDF_FILTER_ID]) {
        if (!sdf->bi_id) {
            sdf->bi_id = kzalloc(sizeof(*sdf->bi_id), GFP_ATOMIC);
            if (!sdf->bi_id) {
                return -ENOMEM;
            }
        }
        *sdf->bi_id = nla_get_u32(attrs[GTP5G_SDF_FILTER_SDF_FILTER_ID]);
    }

    return 0;
}

static int parse_ip_filter_rule(struct sdf_filter *sdf, struct nlattr *a)
{
    struct nlattr *attrs[GTP5G_FLOW_DESCRIPTION_ATTR_MAX + 1];
    struct ip_filter_rule *rule;
    int err;
    int i;

    err = nla_parse_nested(attrs, GTP5G_FLOW_DESCRIPTION_ATTR_MAX, a, NULL, NULL);
    if (err)
        return err;

    if (!attrs[GTP5G_FLOW_DESCRIPTION_ACTION])
        return -EINVAL;
    if (!attrs[GTP5G_FLOW_DESCRIPTION_DIRECTION])
        return -EINVAL;
    if (!attrs[GTP5G_FLOW_DESCRIPTION_PROTOCOL])
        return -EINVAL;
    if (!attrs[GTP5G_FLOW_DESCRIPTION_SRC_IPV4])
        return -EINVAL;
    if (!attrs[GTP5G_FLOW_DESCRIPTION_DEST_IPV4])
        return -EINVAL;

    if (!sdf->rule) {
        sdf->rule = kzalloc(sizeof(*sdf->rule), GFP_ATOMIC);
        if (!sdf->rule)
            return -ENOMEM;
    }
    rule = sdf->rule;

    rule->action = nla_get_u8(attrs[GTP5G_FLOW_DESCRIPTION_ACTION]);
    rule->direction = nla_get_u8(attrs[GTP5G_FLOW_DESCRIPTION_DIRECTION]);
    rule->proto = nla_get_u8(attrs[GTP5G_FLOW_DESCRIPTION_PROTOCOL]);
    rule->src.s_addr = nla_get_be32(attrs[GTP5G_FLOW_DESCRIPTION_SRC_IPV4]);
    rule->dest.s_addr = nla_get_be32(attrs[GTP5G_FLOW_DESCRIPTION_DEST_IPV4]);
    if (attrs[GTP5G_FLOW_DESCRIPTION_SRC_MASK])
        rule->smask.s_addr = nla_get_be32(attrs[GTP5G_FLOW_DESCRIPTION_SRC_MASK]);
    else
        rule->smask.s_addr = -1;

    if (attrs[GTP5G_FLOW_DESCRIPTION_DEST_MASK])
        rule->dmask.s_addr = nla_get_be32(attrs[GTP5G_FLOW_DESCRIPTION_DEST_MASK]);
    else
        rule->dmask.s_addr = -1;

    if (attrs[GTP5G_FLOW_DESCRIPTION_SRC_PORT]) {
        u32 *sport_encode = nla_data(attrs[GTP5G_FLOW_DESCRIPTION_SRC_PORT]);
        rule->sport_num = nla_len(attrs[GTP5G_FLOW_DESCRIPTION_SRC_PORT]) / sizeof(u32);
        if (rule->sport)
            kfree(rule->sport);
        rule->sport = kzalloc(rule->sport_num * sizeof(*rule->sport), GFP_ATOMIC);
        if (!rule->sport)
            return -ENOMEM;

        for (i = 0; i < rule->sport_num; i++) {
            if ((sport_encode[i] & 0xFFFF) <= (sport_encode[i] >> 16)) {
                rule->sport[i].start = (sport_encode[i] & 0xFFFF);
                rule->sport[i].end = (sport_encode[i] >> 16);
            } else {
                rule->sport[i].start = (sport_encode[i] >> 16);
                rule->sport[i].end = (sport_encode[i] & 0xFFFF);
            }
        }
    }

    if (attrs[GTP5G_FLOW_DESCRIPTION_DEST_PORT]) {
        u32 *dport_encode = nla_data(attrs[GTP5G_FLOW_DESCRIPTION_DEST_PORT]);
        rule->dport_num = nla_len(attrs[GTP5G_FLOW_DESCRIPTION_DEST_PORT]) / sizeof(u32);

        if (rule->dport)
            kfree(rule->dport);

        rule->dport = kzalloc(rule->dport_num * sizeof(*rule->dport), GFP_ATOMIC);
        if (!rule->dport)
            return -ENOMEM;

        for (i = 0; i < rule->dport_num; i++) {
            if ((dport_encode[i] & 0xFFFF) <= (dport_encode[i] >> 16)) {
                rule->dport[i].start = (dport_encode[i] & 0xFFFF);
                rule->dport[i].end = (dport_encode[i] >> 16);
            } else {
                rule->dport[i].start = (dport_encode[i] >> 16);
                rule->dport[i].end = (dport_encode[i] & 0xFFFF);
            }
        }
    }

    return 0;
}

static int gtp5g_genl_fill_rule(struct sk_buff *skb, struct ip_filter_rule *rule)
{
    struct nlattr *nest_rule;
    u32 *u32_buf;
    int i;

    u32_buf = kzalloc(0xff * sizeof(u32), GFP_KERNEL);
    if (!u32_buf)
        return -EMSGSIZE;

    nest_rule = nla_nest_start(skb, GTP5G_SDF_FILTER_FLOW_DESCRIPTION);
    if (!nest_rule)
        goto genlmsg_fail;

    if (nla_put_u8(skb, GTP5G_FLOW_DESCRIPTION_ACTION, rule->action))
        goto genlmsg_fail;
    if (nla_put_u8(skb, GTP5G_FLOW_DESCRIPTION_DIRECTION, rule->direction))
        goto genlmsg_fail;
    if (nla_put_u8(skb, GTP5G_FLOW_DESCRIPTION_PROTOCOL, rule->proto))
        goto genlmsg_fail;
    if (nla_put_be32(skb, GTP5G_FLOW_DESCRIPTION_SRC_IPV4, rule->src.s_addr))
        goto genlmsg_fail;
    if (nla_put_be32(skb, GTP5G_FLOW_DESCRIPTION_DEST_IPV4, rule->dest.s_addr))
        goto genlmsg_fail;

    if (rule->smask.s_addr != -1)
        if (nla_put_be32(skb, GTP5G_FLOW_DESCRIPTION_SRC_MASK, rule->smask.s_addr))
            goto genlmsg_fail;

    if (rule->dmask.s_addr != -1)
        if (nla_put_be32(skb, GTP5G_FLOW_DESCRIPTION_DEST_MASK, rule->dmask.s_addr))
            goto genlmsg_fail;

    if (rule->sport_num && rule->sport) {
        for (i = 0; i < rule->sport_num; i++)
            u32_buf[i] = rule->sport[i].start + (rule->sport[i].end << 16);
        if (nla_put(skb, GTP5G_FLOW_DESCRIPTION_SRC_PORT,
                    rule->sport_num * sizeof(u32), u32_buf))
            goto genlmsg_fail;
    }

    if (rule->dport_num && rule->dport) {
        for (i = 0; i < rule->dport_num; i++)
            u32_buf[i] = rule->dport[i].start + (rule->dport[i].end << 16);
        if (nla_put(skb, GTP5G_FLOW_DESCRIPTION_DEST_PORT,
                    rule->dport_num * sizeof(u32), u32_buf))
            goto genlmsg_fail;
    }

    nla_nest_end(skb, nest_rule);
    kfree(u32_buf);
    return 0;
genlmsg_fail:
    kfree(u32_buf);
    return -EMSGSIZE;
}


static int gtp5g_genl_fill_sdf(struct sk_buff *skb, struct sdf_filter *sdf)
{
    struct nlattr *nest_sdf;

    nest_sdf = nla_nest_start(skb, GTP5G_PDI_SDF_FILTER);
    if (!nest_sdf)
        return -EMSGSIZE;

    if (sdf->rule) {
        if (gtp5g_genl_fill_rule(skb, sdf->rule))
            return -EMSGSIZE;
    }

    if (sdf->tos_traffic_class)
        if (nla_put_u16(skb, GTP5G_SDF_FILTER_TOS_TRAFFIC_CLASS, *sdf->tos_traffic_class))
            return -EMSGSIZE;

    if (sdf->security_param_idx)
        if (nla_put_u32(skb, GTP5G_SDF_FILTER_SECURITY_PARAMETER_INDEX, *sdf->security_param_idx))
            return -EMSGSIZE;

    if (sdf->flow_label)
        if (nla_put_u32(skb, GTP5G_SDF_FILTER_FLOW_LABEL, *sdf->flow_label))
            return -EMSGSIZE;

    if (sdf->bi_id)
        if (nla_put_u32(skb, GTP5G_SDF_FILTER_SDF_FILTER_ID, *sdf->bi_id))
            return -EMSGSIZE;

    nla_nest_end(skb, nest_sdf);
    return 0;
}


static int gtp5g_genl_fill_f_teid(struct sk_buff *skb, struct local_f_teid *f_teid)
{
    struct nlattr *nest_f_teid;

    nest_f_teid = nla_nest_start(skb, GTP5G_PDI_F_TEID);
    if (!nest_f_teid)
        return -EMSGSIZE;

    if (nla_put_u32(skb, GTP5G_F_TEID_I_TEID, ntohl(f_teid->teid)))
        return -EMSGSIZE;
    if (nla_put_be32(skb, GTP5G_F_TEID_GTPU_ADDR_IPV4, f_teid->gtpu_addr_ipv4.s_addr))
        return -EMSGSIZE;

    nla_nest_end(skb, nest_f_teid);
    return 0;
}

static int gtp5g_genl_fill_pdi(struct sk_buff *skb, struct pdi *pdi)
{
    struct nlattr *nest_pdi;

    nest_pdi = nla_nest_start(skb, GTP5G_PDR_PDI);
    if (!nest_pdi)
        return -EMSGSIZE;

    if (pdi->ue_addr_ipv4) {
        if (nla_put_be32(skb, GTP5G_PDI_UE_ADDR_IPV4, pdi->ue_addr_ipv4->s_addr))
            return -EMSGSIZE;
    }

    if (pdi->f_teid) {
        if (gtp5g_genl_fill_f_teid(skb, pdi->f_teid))
            return -EMSGSIZE;
    }

    if (pdi->sdf) {
        if (gtp5g_genl_fill_sdf(skb, pdi->sdf))
            return -EMSGSIZE;
    }

    nla_nest_end(skb, nest_pdi);
    return 0;
}


static int gtp5g_genl_fill_pdr(struct sk_buff *skb, u32 snd_portid, u32 snd_seq,
        u32 type, struct pdr *pdr)
{
    void *genlh;

    genlh = genlmsg_put(skb, snd_portid, snd_seq, &gtp5g_genl_family, 0, type);
    if (!genlh)
        goto genlmsg_fail;

    if (nla_put_u16(skb, GTP5G_PDR_ID, pdr->id))
        goto genlmsg_fail;

    if (nla_put_u32(skb, GTP5G_PDR_PRECEDENCE, pdr->precedence))
        goto genlmsg_fail;

    if (pdr->seid) {
        if (nla_put_u64_64bit(skb, GTP5G_PDR_SEID, pdr->seid, 0))
            goto genlmsg_fail;
    }

    if (pdr->outer_header_removal) {
        if (nla_put_u8(skb, GTP5G_OUTER_HEADER_REMOVAL, *pdr->outer_header_removal))
            goto genlmsg_fail;
    }

    if (pdr->far_id) {
        if (nla_put_u32(skb, GTP5G_PDR_FAR_ID, *pdr->far_id))
            goto genlmsg_fail;
    }

    if (pdr->qer_id) {
        if (nla_put_u32(skb, GTP5G_PDR_QER_ID, *pdr->qer_id))
            goto genlmsg_fail;
    }

    if (pdr->role_addr_ipv4.s_addr) {
        if (nla_put_u32(skb, GTP5G_PDR_ROLE_ADDR_IPV4, pdr->role_addr_ipv4.s_addr))
            goto genlmsg_fail;
    }

    if (pdr->pdi) {
        if (gtp5g_genl_fill_pdi(skb, pdr->pdi))
            goto genlmsg_fail;
    }

    genlmsg_end(skb, genlh);
    return 0;
genlmsg_fail:
    genlmsg_cancel(skb, genlh);
    return -EMSGSIZE;
}