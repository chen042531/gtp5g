#ifndef __PDR_H__
#define __PDR_H__

#include <linux/kernel.h>
#include <linux/net.h>
#include <linux/rculist.h>
#include <linux/range.h>
#include <linux/skbuff.h>
#include <linux/un.h>
#include <net/ip.h>

#include "far.h"
#include "qer.h"
#include "urr.h"

#define SEID_U32ID_HEX_STR_LEN 24

struct local_f_teid {
    u32 teid;
    struct in_addr gtpu_addr_ipv4;
};

struct ip_filter_rule {
    uint8_t action;
    uint8_t direction;
    uint8_t proto;
    struct in_addr src;
    struct in_addr smask;
    struct in_addr dest;
    struct in_addr dmask;
    int sport_num;
    struct range *sport;
    int dport_num;
    struct range *dport;
};

struct sdf_filter {
    struct ip_filter_rule *rule;
    uint16_t *tos_traffic_class;
    u32 *security_param_idx;
    u32 *flow_label;
    u32 *bi_id;
};

struct pdi {
    struct in_addr *ue_addr_ipv4;
    struct local_f_teid *f_teid;
    struct sdf_filter *sdf;
};

struct pdr {
    struct hlist_node hlist_id;
    struct hlist_node hlist_i_teid;
    struct hlist_node hlist_addr;
    struct hlist_node hlist_related_far;
    struct hlist_node hlist_related_qer;
    struct hlist_node hlist_related_urr;

    u64 seid;
    u16 id;
    u32 precedence;
    u8 *outer_header_removal;
    struct pdi *pdi;
    u32 *far_id;
    struct far *far;
    u32 *qer_id;
    struct qer *qer;
    u32 *urr_id;
    struct urr *urr;
    
    /* deprecated: AF_UNIX socket for buffer */
    struct sockaddr_un addr_unix;
    struct socket *sock_for_buf;

    u16 af;
    struct in_addr role_addr_ipv4;
    struct sock *sk;
    struct net_device *dev;
    struct rcu_head rcu_head;

    /* Drop Counter */
    u64 ul_drop_cnt;
    u64 dl_drop_cnt;

    /* Packet Statistics */
    u64                     ul_pkt_cnt;
    u64                     dl_pkt_cnt;
    u64                     ul_byte_cnt;
    u64                     dl_byte_cnt;
};

struct rel_qer_node {
    u32 id;
    struct hlist_node qer_id_list;
}

typedef struct list_entry {
    int value;
    struct list_entry *next;
} list_entry_t;

void append(int value, list_entry_t **head)
{
    list_entry_t *direct = *head;
    list_entry_t *prev = NULL;

    list_entry_t *new = malloc(1 * sizeof(list_entry_t));
    new->value = value, new->next = NULL;

    while (direct) {
        prev = direct;           
        direct = direct->next;
    }

    if (prev)
        prev->next = new;
    else
        *head = new;
}

void remove_list_node(List *list, Node *target)
{
    Node *prev = NULL;
    Node *current = list->head;
    // Walk the list
    while (current != target) {
        prev = current;
        current = current->next;
    }
    // Remove the target by updating the head or the previous node.
    if (!prev)
        list->head = target->next;
    else
        prev->next = target->next;
}

extern void pdr_context_delete(struct pdr *);
extern struct pdr *find_pdr_by_id(struct gtp5g_dev *, u64, u16);
extern struct pdr *pdr_find_by_gtp1u(struct gtp5g_dev *, struct sk_buff *,
        unsigned int, u32);
extern struct pdr *pdr_find_by_ipv4(struct gtp5g_dev *, struct sk_buff *,
        unsigned int, __be32);

extern void pdr_append(u64, u16, struct pdr *, struct gtp5g_dev *);
extern void pdr_update_hlist_table(struct pdr *, struct gtp5g_dev *);

extern void unix_sock_client_delete(struct pdr *);
extern int unix_sock_client_new(struct pdr *);
extern int unix_sock_client_update(struct pdr *);

#endif // __PDR_H__
