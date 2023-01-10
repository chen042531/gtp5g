#include <net/genetlink.h>
#include <net/sock.h>
#include <linux/rculist.h>
#include <net/netns/generic.h>

#include "dev.h"
#include "genl.h"
#include "report.h"
#include "genl_report.h"
#include "genl_urr.h"
#include "hash.h"

#include "net.h"
#include "log.h"
#include "pdr.h"
#include "urr.h"

static int gtp5g_genl_fill_volume_measurement(struct sk_buff *, struct urr *);
static int gtp5g_genl_fill_ur(struct sk_buff *, struct urr *);

int gtp5g_genl_get_usage_report(struct sk_buff *skb, struct genl_info *info)
{
    struct gtp5g_dev *gtp;
    struct urr *urr;
    int ifindex;
    int netnsfd;
    u64 seid;
    u32 urr_id;
    struct sk_buff *skb_ack;
    int err;

    printk(">>>>> get_usage_report");

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

    if (info->attrs[GTP5G_URR_SEID]) {
        seid = nla_get_u64(info->attrs[GTP5G_URR_SEID]);
    } else {
        seid = 0;
    }

    if (info->attrs[GTP5G_URR_ID]) {
        urr_id = nla_get_u32(info->attrs[GTP5G_URR_ID]);
    } else {
        rcu_read_unlock();
        return -ENODEV;
    }

    urr = find_urr_by_id(gtp, seid, urr_id);
    if (!urr) {
        rcu_read_unlock();
        return -ENOENT;
    }

    skb_ack = genlmsg_new(NLMSG_GOODSIZE, GFP_ATOMIC);
    if (!skb_ack) {
        rcu_read_unlock();
        return -ENOMEM;
    }

    urr->end_time = ktime_get_real();
    err = gtp5g_genl_fill_usage_reports(skb_ack,
            NETLINK_CB(skb).portid,
            info->snd_seq,
            info->nlhdr->nlmsg_type,
            urr);
    urr->start_time = ktime_get_real();

    if (err) {
        kfree_skb(skb_ack);
        rcu_read_unlock();
        return err;
    }
    rcu_read_unlock();
    printk(">>>>>endg_usage_report");
    return genlmsg_unicast(genl_info_net(info), skb_ack, info->snd_portid); 
}


static int gtp5g_genl_fill_volume_measurement(struct sk_buff *skb, struct urr *urr)
{
    struct nlattr *nest_volume_measurement;

    nest_volume_measurement = nla_nest_start(skb, GTP5G_UR_VOLUME_MEASUREMENT);
    if (!nest_volume_measurement)
        return -EMSGSIZE;

    if (nla_put_u64_64bit(skb, GTP5G_UR_VOLUME_MEASUREMENT_TOVOL, urr->bytes.totalVolume , 0))
        return -EMSGSIZE;
    if (nla_put_u64_64bit(skb, GTP5G_UR_VOLUME_MEASUREMENT_UVOL, urr->bytes.uplinkVolume, 0))
        return -EMSGSIZE;
    if (nla_put_u64_64bit(skb, GTP5G_UR_VOLUME_MEASUREMENT_DVOL, urr->bytes.downlinkVolume, 0))
        return -EMSGSIZE;
    if (nla_put_u64_64bit(skb, GTP5G_UR_VOLUME_MEASUREMENT_TOPACKET, urr->bytes.totalPktNum, 0))
        return -EMSGSIZE;
    if (nla_put_u64_64bit(skb, GTP5G_UR_VOLUME_MEASUREMENT_UPACKET, urr->bytes.uplinkPktNum, 0))
        return -EMSGSIZE;
    if (nla_put_u64_64bit(skb, GTP5G_UR_VOLUME_MEASUREMENT_DPACKET, urr->bytes.downlinkPktNum, 0))
        return -EMSGSIZE;
    nla_nest_end(skb, nest_volume_measurement);

    memset(&urr->bytes, 0, sizeof(struct VolumeMeasurement));

    return 0;
}

static int gtp5g_genl_fill_ur(struct sk_buff *skb, struct urr *urr)
{
    struct nlattr *nest_usage_report;

    nest_usage_report = nla_nest_start(skb, GTP5G_UR);
    if (!nest_usage_report)
        return -EMSGSIZE;

    if (nla_put_u32(skb, GTP5G_UR_URRID, urr->id))
        return -EMSGSIZE;
    if (nla_put_u64_64bit(skb, GTP5G_UR_START_TIME, urr->start_time, 0))
        return -EMSGSIZE;
    if (nla_put_u64_64bit(skb, GTP5G_UR_END_TIME, urr->end_time, 0))
        return -EMSGSIZE;
    if (gtp5g_genl_fill_volume_measurement(skb, urr))
        return -EMSGSIZE;
    nla_nest_end(skb, nest_usage_report);

    return 0;
}

int gtp5g_genl_fill_usage_reports(struct sk_buff *skb, u32 snd_portid, u32 snd_seq,
    u32 type, struct urr *urr)
{
    void *genlh;
    genlh = genlmsg_put(skb, snd_portid, snd_seq,
            &gtp5g_genl_family, 0, type);
    if (!genlh)
        goto genlmsg_fail;

    gtp5g_genl_fill_ur(skb, urr);

    genlmsg_end(skb, genlh);

    return 0;

genlmsg_fail:
    genlmsg_cancel(skb, genlh);
    return -EMSGSIZE;
}