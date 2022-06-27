#include <linux/version.h>
#include <linux/socket.h>
#include <linux/rculist.h>
#include <linux/udp.h>
#include <linux/gtp.h>

#include <net/ip.h>
#include <net/udp.h>
#include <net/udp_tunnel.h>

#include "dev.h"
#include "link.h"
#include "encap.h"
#include "gtp.h"
#include "pdr.h"
#include "far.h"
#include "qer.h"
#include "genl.h"
#include "log.h"
#include "api_version.h"
#include "pktinfo.h"

/* used to compatible with api with/without seid */
#define MSG_URR_BAR_KOV_LEN 4
#define MSG_SEID_KOV_LEN 3
#define MSG_NO_SEID_KOV_LEN 2

enum msg_type {
    TYPE_BUFFER = 1,
    TYPE_URR_REPORT,
    TYPE_BAR_INFO,
};

static void gtp5g_encap_disable_locked(struct sock *);
static int gtp5g_encap_recv(struct sock *, struct sk_buff *);
static int gtp1u_udp_encap_recv(struct gtp5g_dev *, struct sk_buff *);
static int gtp5g_rx(struct pdr *, struct sk_buff *, unsigned int, unsigned int);
static int gtp5g_fwd_skb_encap(struct sk_buff *, struct net_device *,
        unsigned int, struct pdr *);
static int unix_sock_send(struct pdr *, void *, u32);
static int gtp5g_fwd_skb_ipv4(struct sk_buff *, 
    struct net_device *, struct gtp5g_pktinfo *, 
    struct pdr *);

struct sock *gtp5g_encap_enable(int fd, int type, struct gtp5g_dev *gtp){
    struct udp_tunnel_sock_cfg tuncfg = {NULL};
    struct socket *sock;
    struct sock *sk;
    int err;

    GTP5G_LOG(NULL, "enable gtp5g for the fd(%d) type(%d)\n", fd, type);

    sock = sockfd_lookup(fd, &err);
    if (!sock) {
        GTP5G_ERR(NULL, "Failed to find the socket fd(%d)\n", fd);
        return NULL;
    }

    if (sock->sk->sk_protocol != IPPROTO_UDP) {
        GTP5G_ERR(NULL, "socket fd(%d) is not a UDP\n", fd);
        sk = ERR_PTR(-EINVAL);
        goto out_sock;
    }

    lock_sock(sock->sk);
    if (sock->sk->sk_user_data) {
        GTP5G_ERR(NULL, "Failed to set sk_user_datat of socket fd(%d)\n", fd);
        sk = ERR_PTR(-EBUSY);
        goto out_sock;
    }

    sk = sock->sk;
    sock_hold(sk);

    tuncfg.sk_user_data = gtp;
    tuncfg.encap_type = type;
    tuncfg.encap_rcv = gtp5g_encap_recv;
    tuncfg.encap_destroy = gtp5g_encap_disable_locked;

    setup_udp_tunnel_sock(sock_net(sock->sk), sock, &tuncfg);

out_sock:
    release_sock(sock->sk);
    sockfd_put(sock);
    return sk;
}


void gtp5g_encap_disable(struct sock *sk)
{
    struct gtp5g_dev *gtp;

    if (!sk) {
        return;
    }

    lock_sock(sk);
    gtp = sk->sk_user_data;
    if (gtp) {
        gtp->sk1u = NULL;
        udp_sk(sk)->encap_type = 0;
        rcu_assign_sk_user_data(sk, NULL);
        sock_put(sk);
    }
    release_sock(sk);
}

static void gtp5g_encap_disable_locked(struct sock *sk)
{
    rtnl_lock();
    gtp5g_encap_disable(sk);
    rtnl_unlock();
}

static int gtp5g_encap_recv(struct sock *sk, struct sk_buff *skb)
{
    struct gtp5g_dev *gtp;
    int ret = 0;

    gtp = rcu_dereference_sk_user_data(sk);
    if (!gtp) {
        return 1;
    }

    switch (udp_sk(sk)->encap_type) {
    case UDP_ENCAP_GTP1U:
        ret = gtp1u_udp_encap_recv(gtp, skb);
        break;
    default:
        ret = -1; // Should not happen
    }

    switch (ret) {
    case 1:
        GTP5G_ERR(gtp->dev, "Pass up to the process\n");
        break;
    case 0:
        break;
    case -1:
        GTP5G_ERR(gtp->dev, "GTP packet has been dropped\n");
        kfree_skb(skb);
        ret = 0;
        break;
    }

    return ret;
}

// static void gtp5g_handle_echo_req(struct sk_buff *skb, struct gtp5g_dev *gtp)
// {
//     #define GTPV1 0x30
//     #define SEQ_NPDU_PRESENT 0x03
//     #define GTPV1_ECHO_RESPONSE 0x02
//     #define SEQ_NUM_PRESENT 0x02
//     #define NPDU_NUM_PRESENT 0x01
//     #define RECOVERY 14

//     struct rtable *rt;
//     struct flowi4 fl4;

//     struct gtpv1_hdr *gtp1, *req_gtp1;
//     struct gtp1_hdr_opt *gtp1_hdr_opt, *req_gtpOptHdr;
//     struct recovery *recov;

//     struct iphdr *req_iph;
//     unsigned int req_src_ip;
//     unsigned int req_dst_ip;

//     struct udphdr *req_uh;
//     unsigned int req_src_port;
//     unsigned int req_dst_port;

//     /* Reset all headers */
//     // skb_reset_transport_header(skb);
//     // skb_reset_network_header(skb);
//     // skb_reset_mac_header(skb);

//     //ipHdr
//     req_iph = (struct iphdr *)skb_network_header(skb);
//     req_src_ip = (unsigned int)req_iph->saddr;
//     req_dst_ip = (unsigned int)req_iph->daddr;
//     printk(">>>>>> src_ip:%pI4 dst_ip:%pI4", &req_src_ip, &req_dst_ip);

//     //udpHdr
//     req_uh = (struct udphdr *)skb_transport_header(skb);
//     req_src_port = (unsigned int)ntohs(req_uh->source);
//     req_dst_port = (unsigned int)ntohs(req_uh->dest);
//     printk(">>>>>> src_port:%u dst_port:%u", req_src_port, req_dst_port);

//     //gtpHdr
//     req_gtp1 = (struct gtpv1_hdr *)(skb->data + sizeof(struct udphdr));
//     gtp1 = skb_put(skb, sizeof(*gtp1));
//     gtp1->flags = GTPV1 + (req_gtp1->flags & SEQ_NPDU_PRESENT);
//     gtp1->type = GTPV1_ECHO_RESPONSE;
//     gtp1->tid = req_gtp1->tid;

//      printk(">>>>>> gtpHdr success");
//     //gtpOptHdr
//     req_gtpOptHdr = (struct gtp1_hdr_opt *)(req_gtp1 + sizeof(struct gtpv1_hdr));
//     req_gtp1 = skb_put(skb, sizeof(*req_gtp1));
//     if (req_gtp1->flags & SEQ_NUM_PRESENT)
//         gtp1_hdr_opt->seq_number = req_gtpOptHdr->seq_number+ 1;
//     if (req_gtp1->flags & NPDU_NUM_PRESENT)
//         gtp1_hdr_opt->NPDU = req_gtpOptHdr->NPDU;

//     printk(">>>>>> req_gtpOptHdr success");
//     //recovery
//     recov = skb_put(skb, sizeof(*recov));
//     recov->typeNum = RECOVERY;
//     recov->cnt = 0;

//     printk(">>>>>> recovery success");
//     // //switch request Src Addr & Dst Addr
//     rt = ip4_find_route(skb, req_iph, gtp->sk1u, gtp->dev, 
//         req_dst_ip /* Response Src Addr */ ,
//         req_src_ip /* Response Dst Addr*/, 
//         &fl4);
//     if (IS_ERR(rt)) {
//         GTP5G_ERR(gtp->dev, "Failed to send GTP-U echo response due to routing\n");
//         dev_kfree_skb(skb);
//         return;
//     }

//     printk(">>>>>>@@ ip4_find_route success");

//     udp_tunnel_xmit_skb(rt, 
//                 gtp->sk1u, 
//                 skb,
// 			    fl4.saddr, 
//                 fl4.daddr,
// 			    iph->tos,
// 			    ip4_dst_hoplimit(&rt->dst),
// 			    0,
// 			    htons(GTP1U_PORT), 
//                 htons(GTP1U_PORT),
// 			    !net_eq(sock_net(gtp->sk1u),
// 				    dev_net(gtp->dev)),
// 			    false);
//     // udp_tunnel_xmit_skb(rt, 
//     //     gtp->sk1u, 
//     //     skb,
//     //     fl4.saddr, 
//     //     fl4.daddr,
//     //     0,
//     //     ip4_dst_hoplimit(&rt->dst),
//     //     0,
//     //     req_src_port, 
//     //     req_dst_port,
//     //     true, 
//     //     true);

//     return;
// }

#define GTP1U_PORT	2152

#define GTP_ECHO_REQ	1
#define GTP_ECHO_RSP	2
#define GTPIE_RECOVERY	14

#define GTP1_F_NPDU	0x01
#define GTP1_F_SEQ	0x02
#define GTP1_F_EXTHDR	0x04
static struct rtable *ip4_route_output_gtp(struct flowi4 *fl4,
					   const struct sock *sk,
					   __be32 daddr, __be32 saddr)
{
	memset(fl4, 0, sizeof(*fl4));
	fl4->flowi4_oif		= sk->sk_bound_dev_if;
	fl4->daddr		= daddr;
	fl4->saddr		= saddr;
	fl4->flowi4_tos		= RT_CONN_FLAGS(sk);
	fl4->flowi4_proto	= sk->sk_protocol;

	return ip_route_output_key(sock_net(sk), fl4);
}

struct gtp1_header {	/* According to 3GPP TS 29.060. */
	__u8	flags;
	__u8	type;
	__be16	length;
	__be32	tid;
} __attribute__ ((packed));

struct gtp1_header_long {	/* According to 3GPP TS 29.060. */
	__u8	flags;
	__u8	type;
	__be16	length;
	__be32	tid;
	__be16	seq;
	__u8	npdu;
	__u8	next;
} __packed;

/* GTP Information Element */
struct gtp_ie {
	__u8	tag;
	__u8	val;
} __packed;

struct gtp1u_packet {
	struct gtp1_header_long gtp1u_h;
	struct gtp_ie ie;
} __packed;

static int gtp1u_send_echo_resp(struct gtp5g_dev *gtp, struct sk_buff *skb)
{
	struct gtp1_header_long *gtp1u;
	struct gtp1u_packet *gtp_pkt;
	struct rtable *rt;
	struct flowi4 fl4;
	struct iphdr *iph;

	gtp1u = (struct gtp1_header_long *)(skb->data + sizeof(struct udphdr));

	/* 3GPP TS 29.281 5.1 - For the Echo Request, Echo Response,
	 * Error Indication and Supported Extension Headers Notification
	 * messages, the S flag shall be set to 1 and TEID shall be set to 0.
	 */
	if (!(gtp1u->flags & GTP1_F_SEQ) || gtp1u->tid)
		return -1;

	/* pull GTP and UDP headers */
	pskb_pull(skb,
		      sizeof(struct gtp1_header_long) + sizeof(struct udphdr));

	gtp_pkt = skb_push(skb, sizeof(struct gtp1u_packet));
	memset(gtp_pkt, 0, sizeof(struct gtp1u_packet));

	/* S flag must be set to 1 */
	gtp_pkt->gtp1u_h.flags = 0x32;
	gtp_pkt->gtp1u_h.type = GTP_ECHO_RSP;
	/* seq, npdu and next should be counted to the length of the GTP packet
	 * that's why szie of gtp1_header should be subtracted,
	 * not why szie of gtp1_header_long.
	 */
	gtp_pkt->gtp1u_h.length =
		htons(sizeof(struct gtp1u_packet) - sizeof(struct gtp1_header));
	/* 3GPP TS 29.281 5.1 - TEID has to be set to 0 */
	gtp_pkt->gtp1u_h.tid = 0;

	/* 3GPP TS 29.281 7.7.2 - The Restart Counter value in the
	 * Recovery information element shall not be used, i.e. it shall
	 * be set to zero by the sender and shall be ignored by the receiver.
	 * The Recovery information element is mandatory due to backwards
	 * compatibility reasons.
	 */
	gtp_pkt->ie.tag = GTPIE_RECOVERY;
	gtp_pkt->ie.val = 0;

	iph = ip_hdr(skb);

    printk(">>>>> 111");
	/* find route to the sender,
	 * src address becomes dst address and vice versa.
	 */
	rt = ip4_route_output_gtp(&fl4, gtp->sk1u, iph->saddr, iph->daddr);
    // rt = ip4_find_route(skb, iph, gtp->sk1u, gtp->dev, 
    //     iph->saddr /* Response Src Addr */ ,
    //     iph->daddr /* Response Dst Addr*/, 
    //     &fl4);
	if (IS_ERR(rt)) {
		netdev_dbg(gtp->dev, "no route for echo response from %pI4\n",
			   &iph->saddr);
		return -1;
	}

    printk(">>>>> 22");

	udp_tunnel_xmit_skb(rt, gtp->sk1u, skb,
			    fl4.saddr, fl4.daddr,
			    iph->tos,
			    ip4_dst_hoplimit(&rt->dst),
			    0,
			    htons(GTP1U_PORT), htons(GTP1U_PORT),
			    !net_eq(sock_net(gtp->sk1u),
				    dev_net(gtp->dev)),
			    false);
    printk(">>>>> 3");
	return 0;
}

static int gtp1u_udp_encap_recv(struct gtp5g_dev *gtp, struct sk_buff *skb)
{
    unsigned int hdrlen = sizeof(struct udphdr) + sizeof(struct gtpv1_hdr);
    struct gtpv1_hdr *gtpv1;
    struct pdr *pdr;
    int gtpv1_hdr_len;

    if (!pskb_may_pull(skb, hdrlen)) {
        GTP5G_ERR(gtp->dev, "Failed to pull skb length %#x\n", hdrlen);
        return -1;
    }

    gtpv1 = (struct gtpv1_hdr *)(skb->data + sizeof(struct udphdr));
    if ((gtpv1->flags >> 5) != GTP_V1) {
        GTP5G_ERR(gtp->dev, "GTP-U version is not ver1: %#x\n",
            gtpv1->flags);
        return 1;
    }

    if (gtpv1->type != GTP_TPDU && gtpv1->type != GTP_ECHO_REQUEST) {
        GTP5G_ERR(gtp->dev, "GTP-U message type is not a TPDU or GTP echo request: %#x\n",
            gtpv1->type);
        return 1;
    }

    if (gtpv1->type == GTP_ECHO_REQUEST) {
        GTP5G_ERR(gtp->dev, "GTP-U message type is GTP echo request: %#x\n",
            gtpv1->type);
        // gtp5g_handle_echo_req(skb, gtp);
        gtp1u_send_echo_resp(gtp, skb);
        return 0;
    }

    gtpv1_hdr_len = get_gtpu_header_len(gtpv1, skb);
    // pskb_may_pull() may be called in get_gtpu_header_len(), so gtpv1 may be invalidated here.
    if (gtpv1_hdr_len < 0) {
        GTP5G_ERR(gtp->dev, "Invalid extension header length or else\n");
        return -1;
    }

    hdrlen = sizeof(struct udphdr) + gtpv1_hdr_len;
    if (!pskb_may_pull(skb, hdrlen)) {
        GTP5G_ERR(gtp->dev, "Failed to pull skb length %#x\n", hdrlen);
        return -1;
    }
    // pskb_may_pull() is called, so gtpv1 may be invalidated here.

    // recalculation gtpv1
    gtpv1 = (struct gtpv1_hdr *)(skb->data + sizeof(struct udphdr));
    pdr = pdr_find_by_gtp1u(gtp, skb, hdrlen, gtpv1->tid);
    // pskb_may_pull() is called in pdr_find_by_gtp1u(), so gtpv1 may be invalidated here.
    // recalculation gtpv1
    gtpv1 = (struct gtpv1_hdr *)(skb->data + sizeof(struct udphdr));
    if (!pdr) {
        GTP5G_ERR(gtp->dev, "No PDR match this skb : teid[%d]\n", ntohl(gtpv1->tid));
        return -1;
    }

    return gtp5g_rx(pdr, skb, hdrlen, gtp->role);
}

static int gtp5g_drop_skb_encap(struct sk_buff *skb, struct net_device *dev, 
    struct pdr *pdr)
{
    pdr->ul_drop_cnt++;
    dev_kfree_skb(skb);
    return 0;
}

static int gtp5g_buf_skb_encap(struct sk_buff *skb, struct net_device *dev, 
    unsigned int hdrlen, struct pdr *pdr)
{
    // Get rid of the GTP-U + UDP headers.
    if (iptunnel_pull_header(skb,
            hdrlen, 
            skb->protocol,
            !net_eq(sock_net(pdr->sk), dev_net(dev)))) {
        GTP5G_ERR(dev, "Failed to pull GTP-U and UDP headers\n");
        return -1;
    }

    if (unix_sock_send(pdr, skb->data, skb_headlen(skb)) < 0) {
        GTP5G_ERR(dev, "Failed to send skb to unix domain socket PDR(%u)", pdr->id);
        ++pdr->ul_drop_cnt;
    }

    dev_kfree_skb(skb);
    return 0;
}

/* Function unix_sock_{...} are used to handle buffering */
// Send PDR ID, FAR action and buffered packet to user space
static int unix_sock_send(struct pdr *pdr, void *buf, u32 len)
{
    struct msghdr msg;
    struct kvec *kov;

    int msg_kovlen;
    int total_kov_len = 0;
    int i, rt;
    u8  type_hdr[1] = {TYPE_BUFFER};
    u64 self_seid_hdr[1] = {pdr->seid};
    u16 self_hdr[2] = {pdr->id, pdr->far->action};

    if (!pdr->sock_for_buf) {
        GTP5G_ERR(NULL, "Failed Socket buffer is NULL\n");
        return -EINVAL;
    }

    memset(&msg, 0, sizeof(msg));
        if (get_api_with_seid() && get_api_with_urr_bar()) {    
        msg_kovlen = MSG_URR_BAR_KOV_LEN;
        kov = kmalloc_array(msg_kovlen, sizeof(struct kvec),
            GFP_KERNEL);
        if (kov == NULL)
            return -ENOMEM;

        memset(kov, 0, sizeof(struct kvec) * msg_kovlen);

        kov[0].iov_base = type_hdr;
        kov[0].iov_len = sizeof(type_hdr);
        kov[1].iov_base = self_seid_hdr;
        kov[1].iov_len = sizeof(self_seid_hdr);
        kov[2].iov_base = self_hdr;
        kov[2].iov_len = sizeof(self_hdr);
        kov[3].iov_base = buf;
        kov[3].iov_len = len;
    } else if (get_api_with_seid()) {
        msg_kovlen = MSG_SEID_KOV_LEN;
        kov = kmalloc_array(msg_kovlen, sizeof(struct kvec),
            GFP_KERNEL);
        if (kov == NULL)
            return -ENOMEM;

        memset(kov, 0, sizeof(struct kvec) * msg_kovlen);

        kov[0].iov_base = self_seid_hdr;
        kov[0].iov_len = sizeof(self_seid_hdr);
        kov[1].iov_base = self_hdr;
        kov[1].iov_len = sizeof(self_hdr);
        kov[2].iov_base = buf;
        kov[2].iov_len = len;
    } else {
        // for backward compatible
        msg_kovlen = MSG_NO_SEID_KOV_LEN;
        kov = kmalloc_array(msg_kovlen, sizeof(struct kvec),
            GFP_KERNEL);
        if (kov == NULL)
            return -ENOMEM;

        memset(kov, 0, sizeof(struct kvec) * msg_kovlen);

        kov[0].iov_base = self_hdr;
        kov[0].iov_len = sizeof(self_hdr);
        kov[1].iov_base = buf;
        kov[1].iov_len = len;
    }

    for (i = 0; i < msg_kovlen; i++)
        total_kov_len += kov[i].iov_len;

    msg.msg_name = 0;
    msg.msg_namelen = 0;
    iov_iter_kvec(&msg.msg_iter, WRITE, kov, msg_kovlen, total_kov_len);
    msg.msg_control = NULL;
    msg.msg_controllen = 0;
    msg.msg_flags = MSG_DONTWAIT;

    rt = sock_sendmsg(pdr->sock_for_buf, &msg);
    kfree(kov);

    return rt;
}

static int gtp5g_rx(struct pdr *pdr, struct sk_buff *skb,
    unsigned int hdrlen, unsigned int role)
{
    int rt = -1;
    struct far *far = pdr->far;
    // struct qer *qer = pdr->qer;

    if (!far) {
        GTP5G_ERR(pdr->dev, "FAR not exists for PDR(%u)\n", pdr->id);
        goto out;
    }

    //TODO: QER
    //if (qer) {
    //    printk_ratelimited("%s:%d QER Rule found, id(%#x) qfi(%#x)\n", __func__, __LINE__,
    //        qer->id, qer->qfi);
    //}

    // TODO: not reading the value of outer_header_removal now,
    // just check if it is assigned.
    if (pdr->outer_header_removal) {
        // One and only one of the DROP, FORW and BUFF flags shall be set to 1.
        // The NOCP flag may only be set if the BUFF flag is set.
        // The DUPL flag may be set with any of the DROP, FORW, BUFF and NOCP flags.
        switch(far->action & FAR_ACTION_MASK) {
        case FAR_ACTION_DROP: 
            rt = gtp5g_drop_skb_encap(skb, pdr->dev, pdr);
            break;
        case FAR_ACTION_FORW:
            rt = gtp5g_fwd_skb_encap(skb, pdr->dev, hdrlen, pdr);
            break;
        case FAR_ACTION_BUFF:
            rt = gtp5g_buf_skb_encap(skb, pdr->dev, hdrlen, pdr);
            break;
        default:
            GTP5G_ERR(pdr->dev, "Unhandled apply action(%u) in FAR(%u) and related to PDR(%u)\n",
                far->action, far->id, pdr->id);
        }
        goto out;
    } 

    // TODO: this action is not supported
    GTP5G_ERR(pdr->dev, "Uplink: PDR(%u) didn't has a OHR information "
        "(which routed to the gtp interface and matches a PDR)\n", pdr->id);

out:
    return rt;
}

static int gtp5g_fwd_skb_encap(struct sk_buff *skb, struct net_device *dev,
    unsigned int hdrlen, struct pdr *pdr)
{
    struct far *far = pdr->far;
    struct forwarding_parameter *fwd_param = far->fwd_param;
    struct outer_header_creation *hdr_creation;
    struct forwarding_policy *fwd_policy;
    struct gtpv1_hdr *gtp1;
    struct iphdr *iph;
    struct udphdr *uh;
    struct pcpu_sw_netstats *stats;
    int ret;

    if (fwd_param) {
        if ((fwd_policy = fwd_param->fwd_policy))
            skb->mark = fwd_policy->mark;

        if ((hdr_creation = fwd_param->hdr_creation)) {
            // Just modify the teid and packet dest ip
            gtp1 = (struct gtpv1_hdr *)(skb->data + sizeof(struct udphdr));
            gtp1->tid = hdr_creation->teid;

            skb_push(skb, 20); // L3 Header Length
            iph = ip_hdr(skb);

            if (!pdr->pdi->f_teid) {
                GTP5G_ERR(dev, "Failed to hdr removal + creation "
                    "due to pdr->pdi->f_teid not exist\n");
                return -1;
            }
            
            iph->saddr = pdr->pdi->f_teid->gtpu_addr_ipv4.s_addr;
            iph->daddr = hdr_creation->peer_addr_ipv4.s_addr;
            iph->check = 0;

            uh = udp_hdr(skb);
            uh->check = 0;

            if (ip_xmit(skb, pdr->sk, dev) < 0) {
                GTP5G_ERR(dev, "Failed to transmit skb through ip_xmit\n");
                return -1;
            }

            return 0;
        }
    }

    // Get rid of the GTP-U + UDP headers.
    if (iptunnel_pull_header(skb,
            hdrlen, 
            skb->protocol,
            !net_eq(sock_net(pdr->sk), 
            dev_net(dev)))) {
        GTP5G_ERR(dev, "Failed to pull GTP-U and UDP headers\n");
        return -1;
    }

    /* Now that the UDP and the GTP header have been removed, set up the
     * new network header. This is required by the upper layer to
     * calculate the transport header.
     * */
    skb_reset_network_header(skb);

    skb->dev = dev;

    stats = this_cpu_ptr(skb->dev->tstats);
    u64_stats_update_begin(&stats->syncp);
    stats->rx_packets++;
    stats->rx_bytes += skb->len;
    u64_stats_update_end(&stats->syncp);

    pdr->ul_pkt_cnt++;
    pdr->ul_byte_cnt += skb->len; /* length without GTP header */
    GTP5G_LOG(NULL, "PDR (%u) UL_PKT_CNT (%llu) UL_BYTE_CNT (%llu)", pdr->id, pdr->ul_pkt_cnt, pdr->ul_byte_cnt);

    ret = netif_rx(skb);
    if (ret != NET_RX_SUCCESS) {
        GTP5G_ERR(dev, "Uplink: Packet got dropped\n");
    }

    return 0;
}

static int gtp5g_drop_skb_ipv4(struct sk_buff *skb, struct net_device *dev, 
    struct pdr *pdr)
{
    ++pdr->dl_drop_cnt;
    dev_kfree_skb(skb);
    return FAR_ACTION_DROP;
}

static int gtp5g_fwd_skb_ipv4(struct sk_buff *skb, 
    struct net_device *dev, struct gtp5g_pktinfo *pktinfo, 
    struct pdr *pdr)
{
    struct rtable *rt;
    struct flowi4 fl4;
    struct iphdr *iph = ip_hdr(skb);
    struct outer_header_creation *hdr_creation;

    if (!(pdr->far && pdr->far->fwd_param &&
        pdr->far->fwd_param->hdr_creation)) {
        GTP5G_ERR(dev, "Unknown RAN address\n");
        goto err;
    }

    hdr_creation = pdr->far->fwd_param->hdr_creation;
    rt = ip4_find_route(skb, 
        iph, 
        pdr->sk,
        dev, 
        pdr->role_addr_ipv4.s_addr, 
        hdr_creation->peer_addr_ipv4.s_addr, 
        &fl4);
    if (IS_ERR(rt))
        goto err;

    if (!pdr->qer) {
        gtp5g_set_pktinfo_ipv4(pktinfo,
            pdr->sk, 
            iph, 
            hdr_creation,
            NULL, 
            rt, 
            &fl4, 
            dev);
    } else {
        gtp5g_set_pktinfo_ipv4(pktinfo,
            pdr->sk, 
            iph, 
            hdr_creation, 
            pdr->qer, 
            rt, 
            &fl4, 
            dev);
    }

    pdr->dl_pkt_cnt++;
    pdr->dl_byte_cnt += skb->len;
    GTP5G_LOG(NULL, "PDR (%u) DL_PKT_CNT (%llu) DL_BYTE_CNT (%llu)", pdr->id, pdr->dl_pkt_cnt, pdr->dl_byte_cnt);

    gtp5g_push_header(skb, pktinfo);

    return FAR_ACTION_FORW;
err:
    return -EBADMSG;
}

static int gtp5g_buf_skb_ipv4(struct sk_buff *skb, struct net_device *dev,
    struct pdr *pdr)
{
    // TODO: handle nonlinear part
    if (unix_sock_send(pdr, skb->data, skb_headlen(skb)) < 0) {
        GTP5G_ERR(dev, "Failed to send skb to unix domain socket PDR(%u)", pdr->id);
        ++pdr->dl_drop_cnt;
    }

    dev_kfree_skb(skb);
    return FAR_ACTION_BUFF;
}

int gtp5g_handle_skb_ipv4(struct sk_buff *skb, struct net_device *dev,
    struct gtp5g_pktinfo *pktinfo)
{
    struct gtp5g_dev *gtp = netdev_priv(dev);
    struct pdr *pdr;
    struct far *far;
    //struct gtp5g_qer *qer;
    struct iphdr *iph;

    /* Read the IP destination address and resolve the PDR.
     * Prepend PDR header with TEI/TID from PDR.
     */
    iph = ip_hdr(skb);
    if (gtp->role == GTP5G_ROLE_UPF)
        pdr = pdr_find_by_ipv4(gtp, skb, 0, iph->daddr);
    else
        pdr = pdr_find_by_ipv4(gtp, skb, 0, iph->saddr);

    if (!pdr) {
        GTP5G_ERR(dev, "no PDR found for %pI4, skip\n", &iph->daddr);
        return -ENOENT;
    }

    /* TODO: QoS rule have to apply before apply FAR 
     * */
    //qer = pdr->qer;
    //if (qer) {
    //    GTP5G_ERR(dev, "%s:%d QER Rule found, id(%#x) qfi(%#x) TODO\n", 
    //            __func__, __LINE__, qer->id, qer->qfi);
    //}

    far = pdr->far;
    if (far) {
        // One and only one of the DROP, FORW and BUFF flags shall be set to 1.
        // The NOCP flag may only be set if the BUFF flag is set.
        // The DUPL flag may be set with any of the DROP, FORW, BUFF and NOCP flags.
        switch (far->action & FAR_ACTION_MASK) {
        case FAR_ACTION_DROP:
            return gtp5g_drop_skb_ipv4(skb, dev, pdr);
        case FAR_ACTION_FORW:
            return gtp5g_fwd_skb_ipv4(skb, dev, pktinfo, pdr);
        case FAR_ACTION_BUFF:
            return gtp5g_buf_skb_ipv4(skb, dev, pdr);
        default:
            GTP5G_ERR(dev, "Unspec apply action(%u) in FAR(%u) and related to PDR(%u)",
                far->action, far->id, pdr->id);
        }
    }

    return -ENOENT;
}
